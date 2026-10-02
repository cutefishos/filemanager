#pragma once

#include <QStringList>
#include <QUrl>

#include <fileitem.h>

#include <functional>

class FolderModel;
class OperationTask;
class QQuickWindow;
class QWindow;
class WindowManager;

namespace CutefishFM {

/**
 * The file manager's dialogs, created through the window manager of the engine
 * that asked for them. @p owner becomes the transient parent and, except for
 * progress, closes the dialog when it goes away. Each returns nullptr without
 * a manager or when the dialog cannot be created.
 */
namespace Dialogs {

/** Progress and conflicts of @p task; shows itself only if the task takes a while. */
QQuickWindow *ioTask(WindowManager *manager, OperationTask *task);

/** Asks before deleting the selection of @p model permanently. */
QQuickWindow *confirmDelete(WindowManager *manager, FolderModel *model, QWindow *owner);

QQuickWindow *confirmEmptyTrash(WindowManager *manager, QWindow *owner);

QQuickWindow *properties(WindowManager *manager, const Cutefish::IO::FileItemList &items,
                         QWindow *owner);

/** Asks before running the launcher or program at @p path, which is not trusted yet. */
QQuickWindow *confirmLaunch(WindowManager *manager, const QString &path, QWindow *owner);

QQuickWindow *openWith(WindowManager *manager, const QUrl &url, QWindow *owner);

} // namespace Dialogs

/** Opens @p paths as folder windows, selecting @p selection in them. */
using FolderOpener = std::function<void(const QStringList &paths, const QStringList &selection)>;

/**
 * Installed by the file manager application so folders open in-process.
 * Without one, other hosts ask the running file manager over D-Bus and fall
 * back to starting it.
 */
void setFolderOpener(FolderOpener opener);

void openFolders(const QStringList &paths);

} // namespace CutefishFM
