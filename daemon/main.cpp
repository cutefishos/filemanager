#include "fileoperationsservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("cutefish-filemanager-daemon"));

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical() << "Cannot connect to the session D-Bus.";
        return 1;
    }
    if (!bus.registerService(QStringLiteral("com.cutefish.FileOperations"))) {
        qCritical() << bus.lastError().message();
        return 1;
    }

    FileOperationsService service;
    if (!bus.registerObject(QStringLiteral("/com/cutefish/FileOperations"), &service,
                            QDBusConnection::ExportAllSlots
                                    | QDBusConnection::ExportAllSignals)) {
        qCritical() << bus.lastError().message();
        return 1;
    }

    return app.exec();
}
