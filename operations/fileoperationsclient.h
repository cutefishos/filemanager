#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QUrl>

#include <task.h>

class OperationTask;

class FileOperationsClient : public QObject
{
    Q_OBJECT

public:
    static FileOperationsClient *instance();

    OperationTask *submit(const Cutefish::IO::TaskRequest &request);
    OperationTask *copy(const QList<QUrl> &sources, const QUrl &destination);
    OperationTask *move(const QList<QUrl> &sources, const QUrl &destination);
    OperationTask *link(const QList<QUrl> &sources, const QUrl &destination);
    OperationTask *remove(const QList<QUrl> &sources);
    OperationTask *mkdir(const QUrl &destination);
    OperationTask *rename(const QUrl &source, const QUrl &destination);
    OperationTask *trash(const QList<QUrl> &sources);
    OperationTask *restore(const QList<QUrl> &sources);
    OperationTask *emptyTrash();
    OperationTask *paste(const QMimeData *data, const QUrl &destination);
    OperationTask *drop(const QMimeData *data, const QUrl &destination, Qt::DropAction action);
    OperationTask *compress(const QStringList &sources, const QString &outputPath);
    OperationTask *extract(const QString &source, const QString &outputPath);
    OperationTask *undo();

signals:
    void taskAdded(OperationTask *task);

private:
    explicit FileOperationsClient(QObject *parent = nullptr);
    OperationTask *attachRemoteTask(const QString &path);

private slots:
    void remoteTaskAdded(const QString &path);

private:
    QHash<QString, QPointer<OperationTask>> m_remoteTasks;
};
