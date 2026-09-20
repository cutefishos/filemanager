#include "fileoperationsclient.h"

#include "operationtask.h"
#include "cio/archivejob.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QFileInfo>
#include <QMimeData>
#include <QPointer>
#include <QTimer>

#include <cerrno>

namespace {
constexpr auto serviceName = "com.cutefish.FileOperations";
constexpr auto managerPath = "/com/cutefish/FileOperations";
constexpr auto managerInterface = "com.cutefish.FileOperations.Manager1";
constexpr auto taskInterface = "com.cutefish.FileOperations.Task1";

bool daemonAvailable()
{
    if (qEnvironmentVariableIsSet("CUTEFISH_FILEOPERATIONS_IN_PROCESS"))
        return false;

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return false;
    const auto registered = bus.interface()->isServiceRegistered(QString::fromLatin1(serviceName));
    if (registered.isValid() && registered.value())
        return true;
    const auto activatable = bus.interface()->activatableServiceNames();
    return activatable.isValid() && activatable.value().contains(QString::fromLatin1(serviceName));
}

QVariantMap progressMap(const Cutefish::IO::TaskProgress &progress)
{
    return {{QStringLiteral("preparing"), progress.preparing},
            {QStringLiteral("totalsKnown"), progress.totalsKnown},
            {QStringLiteral("processedBytes"), progress.processedBytes},
            {QStringLiteral("totalBytes"), progress.totalBytes},
            {QStringLiteral("processedItems"), progress.processedItems},
            {QStringLiteral("totalItems"), progress.totalItems},
            {QStringLiteral("elapsedMs"), progress.elapsedMs},
            {QStringLiteral("currentPath"), progress.currentPath}};
}

QVariantMap initialSnapshot(const Cutefish::IO::TaskRequest &request)
{
    Cutefish::IO::Task task(request);
    return {{QStringLiteral("operationName"), task.operationName()},
            {QStringLiteral("sourceCount"), task.sourceCount()},
            {QStringLiteral("sourceName"), task.sourceName()},
            {QStringLiteral("destinationName"), task.destinationName()},
            {QStringLiteral("progress"), progressMap(task.progressInfo())}};
}

class LocalOperationTask final : public OperationTask
{
public:
    explicit LocalOperationTask(const Cutefish::IO::TaskRequest &request, QObject *parent)
        : OperationTask(initialSnapshot(request), parent)
        , m_task(Cutefish::IO::TaskController::instance()->submit(request))
    {
        initialize();
    }

    explicit LocalOperationTask(Cutefish::IO::Task *task, QObject *parent)
        : OperationTask(initialSnapshot(task->request()), parent)
        , m_task(task)
    {
        initialize();
    }

    void cancel() override
    {
        if (m_task)
            m_task->cancel();
    }

    void resolveConflict(int policy, bool applyToAll) override
    {
        if (!m_task)
            return;
        applySnapshot({{QStringLiteral("waitingForConflict"), false}});
        m_task->resolveConflict(policy, applyToAll);
    }

private:
    void initialize()
    {
        connect(m_task, &Cutefish::IO::Task::progressInfoChanged, this, [this] {
            applySnapshot({{QStringLiteral("progress"), progressMap(m_task->progressInfo())}});
        });
        connect(m_task, &Cutefish::IO::Task::conflictRequested, this,
                [this](const QString &source, const QString &destination) {
                    applySnapshot({{QStringLiteral("waitingForConflict"), true},
                                   {QStringLiteral("conflictSource"), source},
                                   {QStringLiteral("conflictPath"), destination}});
                    emit conflictRequested(source, destination);
                });
        connect(m_task, &Cutefish::IO::Task::result, this, [this](Cutefish::IO::Task *task) {
            const auto error = task->outcome().error;
            finish({{QStringLiteral("progress"), progressMap(task->progressInfo())},
                    {QStringLiteral("canceled"), task->wasCanceled()},
                    {QStringLiteral("errorCode"), error.code},
                    {QStringLiteral("errorString"), task->errorString()}});
            m_task = nullptr;
        });
    }
    QPointer<Cutefish::IO::Task> m_task;
};

class RemoteOperationTask final : public OperationTask
{
    Q_OBJECT

public:
    RemoteOperationTask(const QString &path, const QVariantMap &snapshot, QObject *parent)
        : OperationTask(snapshot, parent)
        , m_path(path)
    {
        auto bus = QDBusConnection::sessionBus();
        bus.connect(serviceName, m_path, taskInterface, QStringLiteral("Changed"), this,
                    SLOT(changed(QVariantMap)));
        bus.connect(serviceName, m_path, taskInterface, QStringLiteral("ConflictRequested"), this,
                    SIGNAL(conflictRequested(QString,QString)));
        bus.connect(serviceName, m_path, taskInterface, QStringLiteral("Finished"), this,
                    SLOT(finished(QVariantMap)));
        QDBusInterface task(serviceName, m_path, taskInterface, bus);
        QDBusReply<QVariantMap> current = task.call(QStringLiteral("GetSnapshot"));
        if (current.isValid())
            applySnapshot(current.value());

        // A daemon that dies mid-task never sends Finished; fail the task
        // instead of leaving its dialog waiting forever.
        auto *watcher = new QDBusServiceWatcher(serviceName, bus,
                                                QDBusServiceWatcher::WatchForUnregistration, this);
        connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
            finish({{QStringLiteral("finished"), true},
                    {QStringLiteral("errorCode"), int(ECONNRESET)},
                    {QStringLiteral("errorString"), tr("The file operations service stopped.")}});
        });
    }

    void cancel() override
    {
        QDBusInterface task(serviceName, m_path, taskInterface, QDBusConnection::sessionBus());
        task.asyncCall(QStringLiteral("Cancel"));
    }

    void resolveConflict(int policy, bool applyToAll) override
    {
        QDBusInterface task(serviceName, m_path, taskInterface, QDBusConnection::sessionBus());
        task.asyncCall(QStringLiteral("ResolveConflict"), policy, applyToAll);
    }

private slots:
    void changed(const QVariantMap &snapshot) { applySnapshot(snapshot); }
    void finished(const QVariantMap &snapshot) { finish(snapshot); }

private:
    QString m_path;
};

class LocalArchiveOperationTask final : public OperationTask
{
public:
    LocalArchiveOperationTask(int operation, const QStringList &sources,
                              const QString &outputPath, QObject *parent)
        : OperationTask(
                  {{QStringLiteral("operationName"),
                    operation == 0 ? QStringLiteral("compress") : QStringLiteral("extract")},
                   {QStringLiteral("sourceCount"), sources.size()},
                   {QStringLiteral("sourceName"),
                    sources.isEmpty() ? QString() : QFileInfo(sources.first()).fileName()},
                   {QStringLiteral("destinationName"), QFileInfo(outputPath).fileName()},
                   {QStringLiteral("progress"),
                    QVariantMap {{QStringLiteral("preparing"), true},
                                 {QStringLiteral("totalsKnown"), false},
                                 {QStringLiteral("processedItems"), 0},
                                 {QStringLiteral("totalItems"), 100},
                                 {QStringLiteral("currentPath"), QString()}}}},
                  parent)
        , m_job(new ArchiveJob(operation == 0 ? ArchiveJob::Compress : ArchiveJob::Extract,
                               sources, outputPath, this))
    {
        connect(m_job, &ArchiveJob::progressChanged, this, [this](int progress) {
            auto info = progressInfo();
            info.insert(QStringLiteral("preparing"), progress < 0);
            info.insert(QStringLiteral("totalsKnown"), progress >= 0);
            info.insert(QStringLiteral("processedItems"), qMax(0, progress));
            applySnapshot({{QStringLiteral("progress"), info}});
        });
        connect(m_job, &ArchiveJob::currentFileChanged, this, [this](const QString &path) {
            auto info = progressInfo();
            info.insert(QStringLiteral("currentPath"), path);
            applySnapshot({{QStringLiteral("progress"), info}});
        });
        connect(m_job, &ArchiveJob::completed, this,
                [this](bool success, bool canceled, const QString &error,
                       const QString &outputPath) {
                    finish({{QStringLiteral("finished"), true},
                            {QStringLiteral("canceled"), canceled},
                            {QStringLiteral("errorCode"), success || canceled ? 0 : 1},
                            {QStringLiteral("errorString"), error},
                            {QStringLiteral("outputs"),
                             outputPath.isEmpty() ? QStringList()
                                                  : QStringList {outputPath}}});
                });
        m_job->start();
    }

    void cancel() override
    {
        if (m_job)
            m_job->cancel();
    }

    void resolveConflict(int policy, bool applyToAll) override
    {
        Q_UNUSED(policy)
        Q_UNUSED(applyToAll)
    }

private:
    QPointer<ArchiveJob> m_job;
};
} // namespace

FileOperationsClient *FileOperationsClient::instance()
{
    static auto *client = new FileOperationsClient(QCoreApplication::instance());
    return client;
}

#include "fileoperationsclient.moc"

FileOperationsClient::FileOperationsClient(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
    bus.connect(serviceName, managerPath, managerInterface, QStringLiteral("TaskAdded"), this,
                SLOT(remoteTaskAdded(QString)));
    bus.connect(serviceName, managerPath, managerInterface, QStringLiteral("HistoryChanged"), this,
                SLOT(remoteHistoryChanged(bool,bool)));

    // The daemon's history is what counts while it runs; its stacks are gone
    // with it, and the in-process controller takes over.
    auto *watcher = new QDBusServiceWatcher(serviceName, bus,
                                            QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this,
            &FileOperationsClient::queryRemoteHistory);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this] { setHistory(false, false); });

    auto *controller = Cutefish::IO::TaskController::instance();
    connect(controller, &Cutefish::IO::TaskController::historyChanged, this, [this, controller] {
        if (!daemonAvailable())
            setHistory(controller->isUndoAvailable(), controller->isRedoAvailable());
    });

    QTimer::singleShot(0, this, [this] {
        auto bus = QDBusConnection::sessionBus();
        if (!bus.isConnected() || !bus.interface())
            return;
        const auto registered = bus.interface()->isServiceRegistered(
                QString::fromLatin1(serviceName));
        if (!registered.isValid() || !registered.value())
            return;
        queryRemoteHistory();
        QDBusInterface manager(serviceName, managerPath, managerInterface, bus);
        const QDBusReply<QStringList> tasks = manager.call(QStringLiteral("TaskPaths"));
        if (!tasks.isValid())
            return;
        for (const auto &path : tasks.value())
            remoteTaskAdded(path);
    });
}

void FileOperationsClient::queryRemoteHistory()
{
    QDBusInterface manager(serviceName, managerPath, managerInterface,
                           QDBusConnection::sessionBus());
    const QDBusReply<bool> canUndo = manager.call(QStringLiteral("CanUndo"));
    const QDBusReply<bool> canRedo = manager.call(QStringLiteral("CanRedo"));
    setHistory(canUndo.isValid() && canUndo.value(), canRedo.isValid() && canRedo.value());
}

void FileOperationsClient::remoteHistoryChanged(bool canUndo, bool canRedo)
{
    setHistory(canUndo, canRedo);
}

void FileOperationsClient::setHistory(bool canUndo, bool canRedo)
{
    if (m_canUndo == canUndo && m_canRedo == canRedo)
        return;
    m_canUndo = canUndo;
    m_canRedo = canRedo;
    emit historyChanged();
}

OperationTask *FileOperationsClient::attachRemoteTask(const QString &path)
{
    if (m_remoteTasks.contains(path) && m_remoteTasks.value(path))
        return m_remoteTasks.value(path);

    QDBusInterface remote(serviceName, path, taskInterface, QDBusConnection::sessionBus());
    QDBusReply<QVariantMap> snapshot = remote.call(QStringLiteral("GetSnapshot"));
    if (!snapshot.isValid())
        return nullptr;

    auto *task = new RemoteOperationTask(path, snapshot.value(), this);
    m_remoteTasks.insert(path, task);

    // Another live process asked for this task and presents it itself. A task
    // whose submitter is gone is adopted, so it does not run unseen.
    const QString owner = task->owner();
    auto bus = QDBusConnection::sessionBus();
    const bool foreign = !owner.isEmpty() && owner != bus.baseService()
            && bus.interface() && bus.interface()->isServiceRegistered(owner);
    if (!foreign)
        emit taskAdded(task);

    QTimer::singleShot(0, task, [task] { task->replayFinished(); });
    return task;
}

void FileOperationsClient::remoteTaskAdded(const QString &path)
{
    attachRemoteTask(path);
}

OperationTask *FileOperationsClient::submit(const Cutefish::IO::TaskRequest &request)
{
    QStringList sources;
    for (const auto &source : request.sources)
        sources.append(source.toString());

    QDBusReply<QDBusObjectPath> reply;
    if (daemonAvailable()) {
        QDBusInterface manager(serviceName, managerPath, managerInterface,
                               QDBusConnection::sessionBus());
        reply = manager.call(QStringLiteral("Submit"), int(request.operation), sources,
                             request.destination.toString(), int(request.conflict));
    }

    OperationTask *task = nullptr;
    if (reply.isValid()) {
        task = attachRemoteTask(reply.value().path());
    }

    const bool usingRemoteTask = task != nullptr;
    if (!task)
        task = new LocalOperationTask(request, this);

    if (!usingRemoteTask)
        emit taskAdded(task);
    return task;
}

OperationTask *FileOperationsClient::copy(const QList<QUrl> &sources, const QUrl &destination)
{
    return submit({Cutefish::IO::Operation::Copy, sources, destination,
                   Cutefish::IO::ConflictPolicy::Ask});
}

OperationTask *FileOperationsClient::move(const QList<QUrl> &sources, const QUrl &destination)
{
    return submit({Cutefish::IO::Operation::Move, sources, destination,
                   Cutefish::IO::ConflictPolicy::Ask});
}

OperationTask *FileOperationsClient::link(const QList<QUrl> &sources, const QUrl &destination)
{
    return submit({Cutefish::IO::Operation::Link, sources, destination,
                   Cutefish::IO::ConflictPolicy::Ask});
}

OperationTask *FileOperationsClient::remove(const QList<QUrl> &sources)
{
    return submit({Cutefish::IO::Operation::Delete, sources});
}

OperationTask *FileOperationsClient::mkdir(const QUrl &destination)
{
    return submit({Cutefish::IO::Operation::Mkdir, {}, destination});
}

OperationTask *FileOperationsClient::rename(const QUrl &source, const QUrl &destination)
{
    return submit({Cutefish::IO::Operation::Rename, {source}, destination,
                   Cutefish::IO::ConflictPolicy::Ask});
}

OperationTask *FileOperationsClient::trash(const QList<QUrl> &sources)
{
    return submit({Cutefish::IO::Operation::Trash, sources});
}

OperationTask *FileOperationsClient::restore(const QList<QUrl> &sources)
{
    return submit({Cutefish::IO::Operation::Restore, sources, {},
                   Cutefish::IO::ConflictPolicy::Ask});
}

OperationTask *FileOperationsClient::emptyTrash()
{
    return submit({Cutefish::IO::Operation::EmptyTrash});
}

OperationTask *FileOperationsClient::paste(const QMimeData *data, const QUrl &destination)
{
    const bool cut = data && data->hasFormat("application/x-cutefish-cutselection");
    return submit({cut ? Cutefish::IO::Operation::Move : Cutefish::IO::Operation::Copy,
                   data ? data->urls() : QList<QUrl>(), destination,
                   Cutefish::IO::ConflictPolicy::Ask});
}

OperationTask *FileOperationsClient::drop(const QMimeData *data, const QUrl &destination,
                                          Qt::DropAction action)
{
    const auto operation = action == Qt::LinkAction ? Cutefish::IO::Operation::Link
            : action == Qt::MoveAction              ? Cutefish::IO::Operation::Move
                                                    : Cutefish::IO::Operation::Copy;
    return submit({operation, data ? data->urls() : QList<QUrl>(), destination,
                   Cutefish::IO::ConflictPolicy::Ask});
}

OperationTask *FileOperationsClient::compress(const QStringList &sources, const QString &outputPath)
{
    QDBusReply<QDBusObjectPath> reply;
    if (daemonAvailable()) {
        QDBusInterface manager(serviceName, managerPath, managerInterface,
                               QDBusConnection::sessionBus());
        reply = manager.call(QStringLiteral("SubmitArchive"), 0, sources, outputPath);
    }
    OperationTask *task = reply.isValid() ? attachRemoteTask(reply.value().path()) : nullptr;
    const bool usingRemoteTask = task != nullptr;
    if (!task)
        task = new LocalArchiveOperationTask(0, sources, outputPath, this);
    if (!usingRemoteTask)
        emit taskAdded(task);
    return task;
}

OperationTask *FileOperationsClient::extract(const QString &source, const QString &outputPath)
{
    const QStringList sources {source};
    QDBusReply<QDBusObjectPath> reply;
    if (daemonAvailable()) {
        QDBusInterface manager(serviceName, managerPath, managerInterface,
                               QDBusConnection::sessionBus());
        reply = manager.call(QStringLiteral("SubmitArchive"), 1, sources, outputPath);
    }
    OperationTask *task = reply.isValid() ? attachRemoteTask(reply.value().path()) : nullptr;
    const bool usingRemoteTask = task != nullptr;
    if (!task)
        task = new LocalArchiveOperationTask(1, sources, outputPath, this);
    if (!usingRemoteTask)
        emit taskAdded(task);
    return task;
}

OperationTask *FileOperationsClient::undo()
{
    return history("Undo", &Cutefish::IO::TaskController::undo);
}

OperationTask *FileOperationsClient::redo()
{
    return history("Redo", &Cutefish::IO::TaskController::redo);
}

OperationTask *FileOperationsClient::history(const char *method,
                                             Cutefish::IO::Task *(Cutefish::IO::TaskController::*local)())
{
    if (daemonAvailable()) {
        QDBusInterface manager(serviceName, managerPath, managerInterface,
                               QDBusConnection::sessionBus());
        const QDBusReply<QDBusObjectPath> reply = manager.call(QString::fromLatin1(method));
        if (reply.isValid() && !reply.value().path().isEmpty())
            return attachRemoteTask(reply.value().path());
        return nullptr;
    }

    Cutefish::IO::Task *task = (Cutefish::IO::TaskController::instance()->*local)();
    if (!task)
        return nullptr;
    auto *operation = new LocalOperationTask(task, this);
    emit taskAdded(operation);
    return operation;
}
