#include "iojobui.h"
#include "dialogs.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "windowmanager.h"

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

                             WindowManager *manager = WindowManager::defaultManager();
                             if (!manager) {
                                 static bool warned = false;
                                 if (!std::exchange(warned, true))
                                     qWarning("installIoJobUi: no window manager to show progress in");
                                 return;
                             }
                             Dialogs::ioTask(manager, task);
                         });
                     });
}
} // namespace CutefishFM
