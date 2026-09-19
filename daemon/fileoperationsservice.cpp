#include "fileoperationsservice.h"

#include "daemontask.h"
#include "daemonarchivetask.h"

#include <QDBusConnection>
#include <QUrl>

#include <task.h>

FileOperationsService::FileOperationsService(QObject *parent)
    : QObject(parent)
{
}

QDBusObjectPath FileOperationsService::SubmitArchive(int operation, const QStringList &sources,
                                                     const QString &outputPath)
{
    if ((operation != 0 && operation != 1) || sources.isEmpty() || outputPath.isEmpty())
        return {};

    const QString path = QStringLiteral("/com/cutefish/FileOperations/tasks/%1").arg(m_nextTaskId++);
    auto *task = new DaemonArchiveTask(path, operation, sources, outputPath, this);
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

QStringList FileOperationsService::TaskPaths() const
{
    return m_taskPaths;
}

QDBusObjectPath FileOperationsService::Undo()
{
    Cutefish::IO::Task *undo = Cutefish::IO::TaskController::instance()->undo();
    if (!undo)
        return {};

    const QString path = QStringLiteral("/com/cutefish/FileOperations/tasks/%1").arg(m_nextTaskId++);
    auto *task = new DaemonTask(path, undo, this);
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
