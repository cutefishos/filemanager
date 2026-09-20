#pragma once

#include <QObject>
#include <QDBusContext>
#include <QDBusObjectPath>

namespace Cutefish::IO {
class Task;
}

class FileOperationsService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.FileOperations.Manager1")

public:
    explicit FileOperationsService(QObject *parent = nullptr);

public slots:
    QDBusObjectPath Submit(int operation, const QStringList &sources,
                           const QString &destination, int conflictPolicy);
    QDBusObjectPath SubmitArchive(int operation, const QStringList &sources,
                                  const QString &outputPath);
    QDBusObjectPath Undo();
    QDBusObjectPath Redo();
    bool CanUndo() const;
    bool CanRedo() const;
    QStringList TaskPaths() const;

signals:
    void TaskAdded(const QString &path);
    void HistoryChanged(bool canUndo, bool canRedo);

private:
    QString caller() const;
    QDBusObjectPath publish(Cutefish::IO::Task *task);

    quint64 m_nextTaskId = 1;
    QStringList m_taskPaths;
};
