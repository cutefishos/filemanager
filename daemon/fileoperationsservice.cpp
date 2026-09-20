#include "fileoperationsservice.h"

#include "daemontask.h"
#include "daemonarchivetask.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QUrl>

#include <task.h>

FileOperationsService::FileOperationsService(QObject *parent)
    : QObject(parent)
{
    auto *controller = Cutefish::IO::TaskController::instance();
    connect(controller, &Cutefish::IO::TaskController::historyChanged, this, [this, controller] {
        emit HistoryChanged(controller->isUndoAvailable(), controller->isRedoAvailable());
    });
}

QDBusObjectPath FileOperationsService::SubmitArchive(int operation, const QStringList &sources,
                                                     const QString &outputPath)
{
    if ((operation != 0 && operation != 1) || sources.isEmpty() || outputPath.isEmpty())
        return {};

    const QString path = QStringLiteral("/com/cutefish/FileOperations/tasks/%1").arg(m_nextTaskId++);
    auto *task = new DaemonArchiveTask(path, operation, sources, outputPath, this);
    task->setOwner(caller());
    const bool registered = QDBusConnection::sessionBus().registerObject(
            path, task, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    if (!registered) {
        task->Cancel();
        task->deleteLater();
        return {};
    }

    m_taskPaths.append(path);
    connect(task, &DaemonArchiveTask::Finished, this,
            [this, path] { m_taskPaths.removeAll(path); });
    emit TaskAdded(path);
    return QDBusObjectPath(path);
}

// The bus name of the client that asked for the task. Clients use it to show
// progress only for their own tasks, so one paste does not raise a dialog in
// every process attached to the daemon.

QString FileOperationsService::caller() const
{
    return calledFromDBus() ? message().service() : QString();
}

QStringList FileOperationsService::TaskPaths() const
{
    return m_taskPaths;
}

QDBusObjectPath FileOperationsService::Undo()
{
    return publish(Cutefish::IO::TaskController::instance()->undo());
}

QDBusObjectPath FileOperationsService::Redo()
{
    return publish(Cutefish::IO::TaskController::instance()->redo());
}

bool FileOperationsService::CanUndo() const
{
    return Cutefish::IO::TaskController::instance()->isUndoAvailable();
}

bool FileOperationsService::CanRedo() const
{
    return Cutefish::IO::TaskController::instance()->isRedoAvailable();
}

QDBusObjectPath FileOperationsService::publish(Cutefish::IO::Task *history)
{
    if (!history)
        return {};

    const QString path = QStringLiteral("/com/cutefish/FileOperations/tasks/%1").arg(m_nextTaskId++);
    auto *task = new DaemonTask(path, history, this);
    task->setOwner(caller());
    const bool registered = QDBusConnection::sessionBus().registerObject(
            path, task, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    if (!registered) {
        task->Cancel();
        task->deleteLater();
        return {};
    }

    m_taskPaths.append(path);
    connect(task, &DaemonTask::Finished, this, [this, path] { m_taskPaths.removeAll(path); });
    emit TaskAdded(path);
    return QDBusObjectPath(path);
}

QDBusObjectPath FileOperationsService::Submit(int operation, const QStringList &sources,
                                              const QString &destination,
                                              int conflictPolicy)
{
    if (operation < int(Cutefish::IO::Operation::Copy)
        || operation > int(Cutefish::IO::Operation::Size)) {
        return {};
    }
    if (conflictPolicy < int(Cutefish::IO::ConflictPolicy::Fail)
        || conflictPolicy > int(Cutefish::IO::ConflictPolicy::Ask)) {
        return {};
    }

    QList<QUrl> sourceUrls;
    sourceUrls.reserve(sources.size());
    for (const auto &source : sources)
        sourceUrls.append(QUrl(source));

    const auto request = Cutefish::IO::TaskRequest {
        Cutefish::IO::Operation(operation), sourceUrls, QUrl(destination),
        Cutefish::IO::ConflictPolicy(conflictPolicy)};
    const QString path = QStringLiteral("/com/cutefish/FileOperations/tasks/%1").arg(m_nextTaskId++);
    auto *task = new DaemonTask(path, request, this);
    task->setOwner(caller());
    const bool registered = QDBusConnection::sessionBus().registerObject(
            path, task, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    if (!registered) {
        task->deleteLater();
        return {};
    }

    m_taskPaths.append(path);
    connect(task, &DaemonTask::Finished, this, [this, path] { m_taskPaths.removeAll(path); });
    emit TaskAdded(path);
    return QDBusObjectPath(path);
}
