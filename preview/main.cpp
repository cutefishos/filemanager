#include "previewservice.h"
#include "qmltypes.h"
#include <cutefishapplication.h>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

int main(int argc, char **argv)
{
    Cutefish::Application::prepare();
    QApplication app(argc, argv);
    app.setApplicationName("cutefish-filepreview");
    app.setOrganizationName("cutefish");
    app.setQuitOnLastWindowClosed(false);
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return 1;
    if (!bus.registerService("com.cutefish.FilePreview"))
        return 0;
    CutefishFM::registerQmlTypes();
    CutefishFM::initResources();
    PreviewService service;
    if (!service.ready() || !bus.registerObject("/com/cutefish/FilePreview", &service,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        qCritical() << bus.lastError().message();
        return 1;
    }
    return app.exec();
}
