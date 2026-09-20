#pragma once

#include <QObject>
#include <QVariantMap>

class OperationTask : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap progressInfo READ progressInfo NOTIFY progressInfoChanged)
    Q_PROPERTY(QString operationName READ operationName CONSTANT)
    Q_PROPERTY(int sourceCount READ sourceCount CONSTANT)
    Q_PROPERTY(QString sourceName READ sourceName CONSTANT)
    Q_PROPERTY(QString destinationName READ destinationName CONSTANT)
    Q_PROPERTY(bool waitingForConflict READ waitingForConflict NOTIFY progressInfoChanged)
    Q_PROPERTY(QString conflictPath READ conflictPath NOTIFY progressInfoChanged)
    Q_PROPERTY(bool finished READ isFinished NOTIFY finishedChanged)

public:
    explicit OperationTask(const QVariantMap &snapshot, QObject *parent = nullptr);

    QVariantMap progressInfo() const;
    QString operationName() const;
    int sourceCount() const;
    QString sourceName() const;
    QString destinationName() const;
    bool waitingForConflict() const;
    QString conflictPath() const;
    bool isFinished() const;

    Q_INVOKABLE virtual void cancel() = 0;
    Q_INVOKABLE virtual void resolveConflict(int policy) = 0;
    Q_INVOKABLE QString formattedSize(quint64 bytes) const;
    Q_INVOKABLE bool wasCanceled() const;
    Q_INVOKABLE int error() const;
    Q_INVOKABLE QString errorString() const;
    void replayFinished();

signals:
    void progressInfoChanged();
    void conflictRequested(const QString &destination);
    void finishedChanged();
    void result(OperationTask *task);

protected:
    void applySnapshot(const QVariantMap &snapshot);
    void finish(const QVariantMap &snapshot);

    QVariantMap m_snapshot;
    bool m_finished = false;
};
