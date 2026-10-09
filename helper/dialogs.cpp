#include "dialogs.h"
#include "dialogs/fileproperties.h"
#include "model/foldermodel.h"
#include "operations/operationtask.h"
#include "windowmanager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QQuickWindow>

namespace CutefishFM {

static FolderOpener s_folderOpener;

static const QUrl ioTaskDialog(QStringLiteral("qrc:/qml/Dialogs/IoTaskDialog.qml"));
static const QUrl deleteDialog(QStringLiteral("qrc:/qml/Dialogs/DeleteDialog.qml"));
static const QUrl emptyTrashDialog(QStringLiteral("qrc:/qml/Dialogs/EmptyTrashDialog.qml"));
static const QUrl propertiesDialog(QStringLiteral("qrc:/qml/Dialogs/PropertiesDialog.qml"));
static const QUrl launchDialog(QStringLiteral("qrc:/qml/Dialogs/LaunchDialog.qml"));
static const QUrl openWithDialog(QStringLiteral("qrc:/qml/Dialogs/OpenWithDialog.qml"));

namespace Dialogs {

QQuickWindow *ioTask(WindowManager *manager, OperationTask *task)
{
    if (!manager || !task)
        return nullptr;

    QQuickWindow *window = manager->create(ioTaskDialog,
                                           {{QStringLiteral("ioTask"), QVariant::fromValue(task)}});
    if (!window)
        return nullptr;

    // QWindow::close() does nothing for a window that was never shown, so a
    // task that ends inside the show delay would leave its dialog behind.
    // Connected after the dialog's own handler, which shows it for an error.
    QPointer<WindowManager> guard(manager);
    QObject::connect(task, &OperationTask::result, window, [guard, window] {
        if (guard && !window->isVisible())
            guard->release(window);
    });
    return window;
}

QQuickWindow *confirmDelete(WindowManager *manager, FolderModel *model, QWindow *owner)
{
    if (!manager || !model)
        return nullptr;

    return manager->create(deleteDialog,
                           {{QStringLiteral("folderModel"), QVariant::fromValue(model)}},
                           owner, WindowManager::CloseWithOwner);
}

QQuickWindow *confirmEmptyTrash(WindowManager *manager, QWindow *owner)
{
    if (!manager)
        return nullptr;

    return manager->create(emptyTrashDialog, {}, owner, WindowManager::CloseWithOwner);
}

QQuickWindow *properties(WindowManager *manager, const Cutefish::IO::FileItemList &items,
                         QWindow *owner)
{
    if (!manager || items.isEmpty())
        return nullptr;

    // Filled in before the window exists: its size follows the content, and a
    // change after the first expose would resize it visibly.
    auto *details = new FileProperties(items);
    QQuickWindow *window = manager->create(
            propertiesDialog, {{QStringLiteral("details"), QVariant::fromValue(details)}}, owner,
            WindowManager::ShowDeferred | WindowManager::CloseWithOwner);
    if (!window) {
        delete details;
        return nullptr;
    }

    details->setParent(window);
    return window;
}

QQuickWindow *confirmLaunch(WindowManager *manager, const QString &path, QWindow *owner)
{
    if (!manager || path.isEmpty())
        return nullptr;

    const QFileInfo info(path);
    return manager->create(launchDialog,
                           {{QStringLiteral("path"), info.absoluteFilePath()},
                            {QStringLiteral("name"), info.fileName()}},
                           owner, WindowManager::CloseWithOwner);
}

QQuickWindow *openWith(WindowManager *manager, const QUrl &url, QWindow *owner)
{
    if (!manager || !url.isValid())
        return nullptr;

    return manager->create(openWithDialog,
                           {{QStringLiteral("url"), url.isLocalFile() ? url.toLocalFile()
                                                                      : url.toString()}},
                           owner, WindowManager::ShowDeferred | WindowManager::CloseWithOwner);
}

} // namespace Dialogs

void setFolderOpener(FolderOpener opener)
{
    s_folderOpener = std::move(opener);
}

void openFolders(const QStringList &paths)
{
    if (paths.isEmpty())
        return;

    if (s_folderOpener) {
        s_folderOpener(paths, {});
        return;
    }

    // Not activatable: a D-Bus started file manager would also open its cwd.
    QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("com.cutefish.FileManager"), QStringLiteral("/FileManager"),
            QStringLiteral("com.cutefish.FileManager"), QStringLiteral("openFiles"));
    message.setArguments({paths});
    message.setAutoStartService(false);

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                     [paths](QDBusPendingCallWatcher *call) {
                         // Not running, or quitting right now: start a new one.
                         if (call->isError())
                             QProcess::startDetached(QStringLiteral("cutefish-filemanager"), paths);
                         call->deleteLater();
                     });
}

} // namespace CutefishFM
