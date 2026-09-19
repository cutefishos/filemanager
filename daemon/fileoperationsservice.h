#pragma once

#include <QObject>
#include <QDBusObjectPath>

class FileOperationsService : public QObject
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
    QStringList TaskPaths() const;

signals:
    void TaskAdded(const QString &path);

private:
    quint64 m_nextTaskId = 1;
    QStringList m_taskPaths;
};
