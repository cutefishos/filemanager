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
                             // A quick task announced by another process can be over
                             // before this runs: its Finished was queued right behind
                             // TaskAdded. Only a failure still has something to show.
                             if (task->isFinished() && (!task->error() || task->wasCanceled()))
                                 return;

                             auto *window = new Window;
                             window->rootContext()->setContextProperty("ioTask", task);
                             window->load(QUrl("qrc:/qml/Dialogs/IoTaskDialog.qml"));

                             // QWindow::close() is a no-op for a window that was never
                             // shown, so a dialog that finishes inside its show delay
                             // would otherwise outlive the task and keep bindings on it.
                             QObject::connect(task, &OperationTask::result, window, [window] {
                                 QQuickWindow *quickWindow = window->quickWindow();
                                 if (!quickWindow || !quickWindow->isVisible())
                                     window->deleteLater();
                             });
                         });
                     });
}
} // namespace CutefishFM
