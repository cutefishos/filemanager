#include "iojobui.h"
#include "window.h"
#include <QCoreApplication>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <task.h>

namespace CutefishFM {
void installIoJobUi()
{
    static bool installed = false;
    if (installed)
        return;
    installed = true;
    auto *controller = Cutefish::IO::TaskController::instance();
    QObject::connect(controller, &Cutefish::IO::TaskController::taskAdded, controller,
                     [](Cutefish::IO::Task *task) {
                         if (task->request().operation == Cutefish::IO::Operation::Size)
                             return;
                         // Defer QML construction so taskAdded cannot reenter a partially created
                         // folder model.
                         QTimer::singleShot(0, task, [task] {
                             auto *window = new Window;
                             window->rootContext()->setContextProperty("ioTask", task);
                             window->load(QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));
                             if (!window->quickWindow())
                                 task->cancel();
                         });
                     });
}
} // namespace CutefishFM
