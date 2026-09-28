#pragma once

#include <QObject>
#include <QVariantMap>

#include <functional>

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
    void setOwner(const QString &owner);
    QString owner() const;
    // The submitter left the bus. A task waiting at a conflict is cancelled;
    // one still running is cancelled at its next conflict unless a client
    // adopts it first.
    void orphan();
    bool isFinished() const;
    // Whether GetSnapshot was called since the task finished: the client saw
    // the outcome, so the object can leave the bus.
    bool outcomeRead() const;
    void setOutcomeReadCallback(std::function<void()> callback);

public slots:
    QVariantMap GetSnapshot() const;
    void Cancel();
    void ResolveConflict(int policy, bool applyToAll);

signals:
    void Changed(const QVariantMap &snapshot);
    void ConflictRequested(const QString &source, const QString &destination);
    void Finished(const QVariantMap &snapshot);

private:
    void initialize();
    QVariantMap progressSnapshot() const;

    QString m_objectPath;
    Cutefish::IO::Task *m_task;
    QVariantMap m_snapshot;
    bool m_finished = false;
    mutable bool m_outcomeRead = false;
    std::function<void()> m_outcomeReadCallback;
};
