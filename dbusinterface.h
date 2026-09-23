#ifndef DBUSINTERFACE_H
#define DBUSINTERFACE_H

#include <QObject>

class Application;

/** org.freedesktop.FileManager1, served by the running file manager itself. */
class DBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")

public:
    explicit DBusInterface(Application *application);

    Q_SCRIPTABLE void ShowFolders(const QStringList& uriList, const QString& startUpId);
    Q_SCRIPTABLE void ShowItems(const QStringList& uriList, const QString& startUpId);
    Q_SCRIPTABLE void ShowItemProperties(const QStringList& uriList, const QString& startUpId);

private:
    Application *m_application;
};

#endif // DBUSINTERFACE_H
