#include "createfolderdialog.h"
#include <QQmlApplicationEngine>
#include <QQuickView>
#include <QQmlContext>

#include <KIO/MkdirJob>

CreateFolderDialog::CreateFolderDialog(QObject *parent)
    : QObject(parent)
{

}

void CreateFolderDialog::setPath(const QString &path)
{
    m_path = path;
}

void CreateFolderDialog::show()
{
    QQmlApplicationEngine *engine = new QQmlApplicationEngine;
    engine->rootContext()->setContextProperty("main", this);
    engine->load(QUrl("qrc:/qml/Dialogs/CreateFolderDialog.qml"));
}

void CreateFolderDialog::newFolder(const QString &folderName)
{
    if (m_path.isEmpty() || folderName.isEmpty())
        return;

    auto job = KIO::mkdir(QUrl(m_path + "/" + folderName));
    job->start();
}
