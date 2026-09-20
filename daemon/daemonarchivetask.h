#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantMap>

class ArchiveJob;

class DaemonArchiveTask : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.FileOperations.Task1")

public:
    DaemonArchiveTask(const QString &objectPath, int operation, const QStringList &sources,
                      const QString &outputPath, QObject *parent = nullptr);
    QString objectPath() const;
    void setOwner(const QString &owner);

public slots:
    QVariantMap GetSnapshot() const;
    void Cancel();
    void ResolveConflict(int policy, bool applyToAll);

signals:
    void Changed(const QVariantMap &snapshot);
    void ConflictRequested(const QString &source, const QString &destination);
    void Finished(const QVariantMap &snapshot);

private:
    QString m_objectPath;
    QPointer<ArchiveJob> m_job;
    QVariantMap m_snapshot;
};
