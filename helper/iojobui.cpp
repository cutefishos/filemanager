#include "iojobui.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "window.h"
#include <QCoreApplication>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

namespace CutefishFM {
void installIoJobUi()
{
    static bool installed = false;
    if (installed)
        return;
    installed = true;
    auto *controller = FileOperationsClient::instance();
    QObject::connect(controller, &FileOperationsClient::taskAdded, controller,
                     [](OperationTask *task) {
                         // Defer QML construction so taskAdded cannot reenter a partially created
                         // folder model.
                         QTimer::singleShot(0, task, [task] {
                             auto *window = new Window;
                             window->rootContext()->setContextProperty("ioTask", task);
                             window->load(QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
                         });
                     });
}
} // namespace CutefishFM
