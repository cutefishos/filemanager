#pragma once

#include <QObject>
#include <QVariantMap>

#include <task.h>

class DaemonTask : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.FileOperations.Task1")

public:
    DaemonTask(const QString &objectPath, const Cutefish::IO::TaskRequest &request,
               QObject *parent = nullptr);
    DaemonTask(const QString &objectPath, Cutefish::IO::Task *task, QObject *parent = nullptr);

    QString objectPath() const;

public slots:
    QVariantMap GetSnapshot() const;
    void Cancel();
    void ResolveConflict(int policy);

signals:
    void Changed(const QVariantMap &snapshot);
    void ConflictRequested(const QString &destination);
    void Finished(const QVariantMap &snapshot);

private:
    void initialize();
    QVariantMap progressSnapshot() const;

    QString m_objectPath;
    Cutefish::IO::Task *m_task;
    QVariantMap m_snapshot;
};
