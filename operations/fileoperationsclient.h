#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QUrl>

#include <task.h>

class OperationTask;

class FileOperationsClient : public QObject
{
    Q_OBJECT

public:
    static FileOperationsClient *instance();

    /**
     * Every task returned below or announced through taskAdded is deleted once
     * its result has been emitted; keep a QPointer to use one after that.
     */
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
    OperationTask *redo();

    bool undoAvailable() const { return m_canUndo; }
    bool redoAvailable() const { return m_canRedo; }

    /** Tasks this process presents (see taskAdded) that have not finished yet. */
    int activeTaskCount() const { return m_activeTasks.size(); }

signals:
    void taskAdded(OperationTask *task);
    void historyChanged();
    void activeTaskCountChanged(int count);

private:
    explicit FileOperationsClient(QObject *parent = nullptr);
    OperationTask *attachRemoteTask(const QString &path);
    OperationTask *history(const char *method, Cutefish::IO::Task *(Cutefish::IO::TaskController::*local)());

private slots:
    void remoteTaskAdded(const QString &path);
    void remoteHistoryChanged(bool canUndo, bool canRedo);

private:
    void setHistory(bool canUndo, bool canRedo);
    void queryRemoteHistory();
    void announce(OperationTask *task);
    void retire(OperationTask *task);
    OperationTask *deleteWhenFinished(OperationTask *task);

    QHash<QString, QPointer<OperationTask>> m_remoteTasks;
    QSet<OperationTask *> m_activeTasks;
    bool m_canUndo = false;
    bool m_canRedo = false;
};
