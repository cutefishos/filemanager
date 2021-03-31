#ifndef CREATEFOLDERDIALOG_H
#define CREATEFOLDERDIALOG_H

#include <QObject>

class CreateFolderDialog : public QObject
{
    Q_OBJECT

public:
    explicit CreateFolderDialog(QObject *parent = nullptr);

    void setPath(const QString &path);
    void show();

    Q_INVOKABLE void newFolder(const QString &folderName);

private:
    QString m_path;
};

#endif // CREATEFOLDERDIALOG_H
