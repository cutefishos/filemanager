#include "dbusinterface.h"
#include "application.h"

#include <QDBusConnection>
#include <QUrl>

static QList<QUrl> toUrls(const QStringList &uriList)
{
    QList<QUrl> urls;
    for (const QString &uri : uriList) {
        const QUrl url = QUrl::fromUserInput(uri, QString(), QUrl::AssumeLocalFile);
        if (url.isValid())
            urls.append(url);
    }
    return urls;
}

DBusInterface::DBusInterface(Application *application)
    : QObject(application)
    , m_application(application)
{
    QDBusConnection::sessionBus().registerObject("/org/freedesktop/FileManager1", this,
                                                 QDBusConnection::ExportScriptableContents | QDBusConnection::ExportAdaptors);
}

void DBusInterface::ShowFolders(const QStringList &uriList, const QString &startUpId)
{
    Q_UNUSED(startUpId);

    QStringList folders;
    for (const QUrl &url : toUrls(uriList))
        folders.append(url.isLocalFile() ? url.toLocalFile() : url.toString());
    m_application->openFiles(folders);
}

void DBusInterface::ShowItems(const QStringList &uriList, const QString &startUpId)
{
    Q_UNUSED(startUpId);

    m_application->showItems(toUrls(uriList));
}

void DBusInterface::ShowItemProperties(const QStringList &uriList, const QString &startUpId)
{
    Q_UNUSED(startUpId);

    m_application->showItemProperties(toUrls(uriList));
}
