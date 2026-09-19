#include "daemonarchivetask.h"

#include "cio/archivejob.h"

#include <QFileInfo>

DaemonArchiveTask::DaemonArchiveTask(const QString &objectPath, int operation,
                                     const QStringList &sources, const QString &outputPath,
                                     QObject *parent)
    : QObject(parent)
    , m_objectPath(objectPath)
    , m_job(new ArchiveJob(operation == 0 ? ArchiveJob::Compress : ArchiveJob::Extract,
                           sources, outputPath, this))
{
    m_snapshot = {
        {QStringLiteral("operationName"), operation == 0 ? QStringLiteral("compress")
                                                          : QStringLiteral("extract")},
        {QStringLiteral("sourceCount"), sources.size()},
        {QStringLiteral("sourceName"),
         sources.isEmpty() ? QString() : QFileInfo(sources.first()).fileName()},
        {QStringLiteral("destinationName"), QFileInfo(outputPath).fileName()},
        {QStringLiteral("progress"),
         QVariantMap {{QStringLiteral("preparing"), true},
                      {QStringLiteral("totalsKnown"), false},
                      {QStringLiteral("processedItems"), 0},
                      {QStringLiteral("totalItems"), 100},
                      {QStringLiteral("currentPath"), QString()}}},
        {QStringLiteral("finished"), false},
        {QStringLiteral("waitingForConflict"), false},
        {QStringLiteral("canceled"), false},
        {QStringLiteral("errorCode"), 0},
        {QStringLiteral("errorString"), QString()},
        {QStringLiteral("outputs"), QStringList()}};

    connect(m_job, &ArchiveJob::progressChanged, this, [this](int progress) {
        auto info = m_snapshot.value(QStringLiteral("progress")).toMap();
        info.insert(QStringLiteral("preparing"), progress < 0);
        info.insert(QStringLiteral("totalsKnown"), progress >= 0);
        info.insert(QStringLiteral("processedItems"), qMax(0, progress));
        m_snapshot.insert(QStringLiteral("progress"), info);
        emit Changed({{QStringLiteral("progress"), info}});
    });
    connect(m_job, &ArchiveJob::currentFileChanged, this, [this](const QString &path) {
        auto info = m_snapshot.value(QStringLiteral("progress")).toMap();
        info.insert(QStringLiteral("currentPath"), path);
        m_snapshot.insert(QStringLiteral("progress"), info);
        emit Changed({{QStringLiteral("progress"), info}});
    });
    connect(m_job, &ArchiveJob::completed, this,
            [this](bool success, bool canceled, const QString &error, const QString &outputPath) {
                m_snapshot.insert(QStringLiteral("finished"), true);
                m_snapshot.insert(QStringLiteral("canceled"), canceled);
                m_snapshot.insert(QStringLiteral("errorCode"), success || canceled ? 0 : 1);
                m_snapshot.insert(QStringLiteral("errorString"), error);
                m_snapshot.insert(QStringLiteral("outputs"),
                                  outputPath.isEmpty() ? QStringList() : QStringList {outputPath});
                emit Finished(m_snapshot);
            });
    connect(m_job, &QThread::finished, m_job, &QObject::deleteLater);
    m_job->start();
}

QString DaemonArchiveTask::objectPath() const
{
    return m_objectPath;
}

QVariantMap DaemonArchiveTask::GetSnapshot() const
{
    return m_snapshot;
}

void DaemonArchiveTask::Cancel()
{
    if (m_job)
        m_job->cancel();
}

void DaemonArchiveTask::ResolveConflict(int policy)
{
    Q_UNUSED(policy)
}

