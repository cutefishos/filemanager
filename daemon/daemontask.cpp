#include "daemontask.h"

namespace {
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
} // namespace

DaemonTask::DaemonTask(const QString &objectPath, const Cutefish::IO::TaskRequest &request,
                       QObject *parent)
    : QObject(parent)
    , m_objectPath(objectPath)
    , m_task(Cutefish::IO::TaskController::instance()->submit(request))
{
    initialize();
}

DaemonTask::DaemonTask(const QString &objectPath, Cutefish::IO::Task *task, QObject *parent)
    : QObject(parent)
    , m_objectPath(objectPath)
    , m_task(task)
{
    initialize();
}

void DaemonTask::initialize()
{
    m_snapshot = {{QStringLiteral("operationName"), m_task->operationName()},
                  {QStringLiteral("sourceCount"), m_task->sourceCount()},
                  {QStringLiteral("sourceName"), m_task->sourceName()},
                  {QStringLiteral("destinationName"), m_task->destinationName()},
                  {QStringLiteral("progress"), progressMap(m_task->progressInfo())},
                  {QStringLiteral("finished"), false},
                  {QStringLiteral("waitingForConflict"), false},
                  {QStringLiteral("conflictPath"), QString()},
                  {QStringLiteral("canceled"), false},
                  {QStringLiteral("errorCode"), 0},
                  {QStringLiteral("errorString"), QString()}};

    connect(m_task, &Cutefish::IO::Task::progressInfoChanged, this, [this] {
        const auto update = progressSnapshot();
        m_snapshot.insert(QStringLiteral("progress"), update.value(QStringLiteral("progress")));
        emit Changed(update);
    });
    connect(m_task, &Cutefish::IO::Task::conflictRequested, this,
            [this](const QString &destination) {
                m_snapshot.insert(QStringLiteral("waitingForConflict"), true);
                m_snapshot.insert(QStringLiteral("conflictPath"), destination);
                emit Changed({{QStringLiteral("waitingForConflict"), true},
                              {QStringLiteral("conflictPath"), destination}});
                emit ConflictRequested(destination);
            });
    connect(m_task, &Cutefish::IO::Task::result, this, [this](Cutefish::IO::Task *task) {
        const auto error = task->outcome().error;
        m_snapshot.insert(QStringLiteral("progress"), progressMap(task->progressInfo()));
        m_snapshot.insert(QStringLiteral("finished"), true);
        m_snapshot.insert(QStringLiteral("waitingForConflict"), false);
        m_snapshot.insert(QStringLiteral("canceled"), task->wasCanceled());
        m_snapshot.insert(QStringLiteral("errorCode"), error.code);
        m_snapshot.insert(QStringLiteral("errorString"), task->errorString());
        emit Finished(m_snapshot);
        m_task = nullptr;
    });
}

QString DaemonTask::objectPath() const
{
    return m_objectPath;
}

void DaemonTask::setOwner(const QString &owner)
{
    m_snapshot.insert(QStringLiteral("owner"), owner);
}

QVariantMap DaemonTask::GetSnapshot() const
{
    return m_snapshot;
}

void DaemonTask::Cancel()
{
    if (m_task)
        m_task->cancel();
}

void DaemonTask::ResolveConflict(int policy)
{
    if (m_task) {
        m_snapshot.insert(QStringLiteral("waitingForConflict"), false);
        emit Changed({{QStringLiteral("waitingForConflict"), false}});
        m_task->resolveConflict(policy);
    }
}

QVariantMap DaemonTask::progressSnapshot() const
{
    return {{QStringLiteral("progress"), progressMap(m_task->progressInfo())}};
}
