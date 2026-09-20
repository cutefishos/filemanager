#include "operationtask.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QLocale>

OperationTask::OperationTask(const QVariantMap &snapshot, QObject *parent)
    : QObject(parent)
    , m_snapshot(snapshot)
{
}

QVariantMap OperationTask::progressInfo() const
{
    const QVariant progress = m_snapshot.value(QStringLiteral("progress"));
    if (progress.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<QVariantMap>(progress.value<QDBusArgument>());
    return progress.toMap();
}

QString OperationTask::operationName() const
{
    return m_snapshot.value(QStringLiteral("operationName")).toString();
}

int OperationTask::sourceCount() const
{
    return m_snapshot.value(QStringLiteral("sourceCount")).toInt();
}

QString OperationTask::sourceName() const
{
    return m_snapshot.value(QStringLiteral("sourceName")).toString();
}

QString OperationTask::destinationName() const
{
    return m_snapshot.value(QStringLiteral("destinationName")).toString();
}

bool OperationTask::waitingForConflict() const
{
    return m_snapshot.value(QStringLiteral("waitingForConflict")).toBool();
}

QString OperationTask::conflictPath() const
{
    return m_snapshot.value(QStringLiteral("conflictPath")).toString();
}

bool OperationTask::isFinished() const
{
    return m_finished;
}

// Bus name of the process that submitted a daemon task; empty for local ones.

QString OperationTask::owner() const
{
    return m_snapshot.value(QStringLiteral("owner")).toString();
}

QString OperationTask::formattedSize(quint64 bytes) const
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeSIFormat);
}

bool OperationTask::wasCanceled() const
{
    return m_snapshot.value(QStringLiteral("canceled")).toBool();
}

int OperationTask::error() const
{
    return m_snapshot.value(QStringLiteral("errorCode")).toInt();
}

QString OperationTask::errorString() const
{
    return m_snapshot.value(QStringLiteral("errorString")).toString();
}

void OperationTask::replayFinished()
{
    if (m_snapshot.value(QStringLiteral("finished")).toBool())
        finish(m_snapshot);
}

void OperationTask::applySnapshot(const QVariantMap &snapshot)
{
    if (snapshot.contains(QStringLiteral("progress")))
        m_snapshot.insert(QStringLiteral("progress"), snapshot.value(QStringLiteral("progress")));
    for (auto i = snapshot.constBegin(); i != snapshot.constEnd(); ++i) {
        if (i.key() != QLatin1String("progress"))
            m_snapshot.insert(i.key(), i.value());
    }
    emit progressInfoChanged();
}

void OperationTask::finish(const QVariantMap &snapshot)
{
    if (m_finished)
        return;
    m_finished = true;
    applySnapshot(snapshot);
    emit finishedChanged();
    emit result(this);
}
