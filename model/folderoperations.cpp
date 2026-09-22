#include "foldermodel.h"
#include "dialogs/filepropertiesdialog.h"
#include "dialogs/openwithdialog.h"
#include "helper/filelauncher.h"
#include "helper/fm.h"
#include "mimetype/xdgdesktopfile.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "window.h"

#include <QApplication>
#include <QClipboard>
#include <QDBusInterface>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QQmlContext>
#include <QTextStream>
#include <sys/stat.h>

static QString uniquePath(const QString &directory, const QString &name, bool directoryPath)
{
    QString stem = name;
    QString suffix;

    if (!directoryPath) {
        const QFileInfo info(name);
        if (!info.suffix().isEmpty()) {
            suffix = QStringLiteral(".") + info.suffix();
            stem.chop(suffix.size());
        }
    }

    QString candidate = QDir(directory).filePath(name);
    int index = 1;
    while (QFileInfo::exists(candidate)) {
        candidate = QDir(directory).filePath(
                QStringLiteral("%1 %2%3").arg(stem).arg(index++).arg(suffix));
    }

    return candidate;
}

static bool isZipFile(const Cutefish::IO::FileItem &item)
{
    if (!item.isLocalFile())
        return false;

    const QString mimeType = item.mimetype();
    return mimeType == QLatin1String("application/zip")
            || mimeType == QLatin1String("application/x-zip-compressed")
            || item.url().path().endsWith(QLatin1String(".zip"), Qt::CaseInsensitive);
}

static bool sendSystemNotification(const QString &summary, const QString &body)
{
    QDBusInterface interface(QStringLiteral("org.freedesktop.Notifications"),
                             QStringLiteral("/org/freedesktop/Notifications"),
                             QStringLiteral("org.freedesktop.Notifications"),
                             QDBusConnection::sessionBus());
    if (!interface.isValid())
        return false;

    QList<QVariant> arguments;
    arguments << QStringLiteral("cutefish-filemanager");
    arguments << static_cast<uint>(0);
    arguments << QStringLiteral("system-file-manager");
    arguments << summary;
    arguments << body;
    arguments << QStringList();
    arguments << QVariantMap();
    arguments << 5000;
    interface.asyncCallWithArgumentList(QStringLiteral("Notify"), arguments);
    return true;
}

QAction *FolderModel::action(const QString &name) const
{
    return m_actions.value(name);
}

void FolderModel::undo()
{
    FileOperationsClient::instance()->undo();
}

void FolderModel::redo()
{
    FileOperationsClient::instance()->redo();
}

bool FolderModel::supportSetAsWallpaper(const QString &mimeType)
{
    if (mimeType == "image/jpeg" || mimeType == "image/png")
        return true;

    return false;
}

void FolderModel::newFolder()
{
    QString rootPath = rootItem().url().toString();
    QString baseName = tr("New Folder");
    QString newName = baseName;

    int i = 0;
    while (true) {
        if (QFile::exists(rootItem().url().toLocalFile() + "/" + newName)) {
            ++i;
            newName = QString("%1%2").arg(baseName).arg(QString::number(i));
        } else {
            break;
        }
    }

    m_newDocumentUrl = QUrl(rootItem().url().toString() + "/" + newName);

    rememberNewDocumentPosition(newName);

    FileOperationsClient::instance()->mkdir(
            QUrl(rootItem().url().toString() + "/" + newName));
}

void FolderModel::newTextFile()
{
    QString rootPath = rootItem().url().toString();
    QString baseName = tr("New Text");
    QString newName = baseName;

    int i = 0;
    while (true) {
        if (QFile::exists(rootItem().url().toLocalFile() + "/" + newName)) {
            ++i;
            newName = QString("%1%2").arg(baseName).arg(QString::number(i));
        } else {
            break;
        }
    }

    m_newDocumentUrl = QUrl(rootItem().url().toString() + "/" + newName);

    rememberNewDocumentPosition(newName);

    QFile file(m_newDocumentUrl.toLocalFile());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << "\n";
        ::chmod(m_newDocumentUrl.toLocalFile().toStdString().c_str(), 0700);
        file.close();
    }
}

void FolderModel::rename(int row, const QString &name)
{
    if (row < 0)
        return;

    if (name.isEmpty() || name == "." || name == ".." || name.contains('/')
        || name.contains(QChar(0))) {
        return;
    }

    const QUrl source = itemForIndex(index(row, 0)).url();
    if (!source.isLocalFile())
        return;

    const QUrl destination = QUrl::fromLocalFile(
            QFileInfo(source.toLocalFile()).absolutePath() + '/' + name);

    expectRename(source, destination);
    FileOperationsClient::instance()->rename(source, destination);
}

void FolderModel::copy()
{
    if (!m_selectionModel->hasSelection())
        return;

    if (QAction *action = m_actions.value("copy"))
        if (!action->isEnabled())
            return;

    QMimeData *mimeData = FolderContentsModel::mimeData(m_selectionModel->selectedIndexes());
    QApplication::clipboard()->setMimeData(mimeData);
}

void FolderModel::paste()
{
    // The clipboard has no mime data at all when nothing owns it.
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    bool enable = false;

    // Update paste action
    if (QAction *paste = m_actions.value(QStringLiteral("paste"))) {
        QList<QUrl> urls = mimeData ? mimeData->urls() : QList<QUrl>();
        const QString &currentUrl = rootItem().url().toLocalFile();

        if (!urls.isEmpty()) {
            if (!rootItem().isNull()) {
                enable = rootItem().isWritable();
            }
        }

        if (enable && !urls.isEmpty()) {
            for (QUrl url : urls) {
                m_needSelectUrls.append(
                        QUrl::fromLocalFile(QString("%1/%2").arg(currentUrl).arg(url.fileName())));
            }
        }

        paste->setEnabled(enable);
    }

    if (enable) {
        // Copy a new MimeData.
        QMimeData *data = new QMimeData;
        for (QString mimetype : mimeData->formats()) {
            data->setData(mimetype, mimeData->data(mimetype));
        }

        FileOperationsClient::instance()->paste(data, m_dirModel->dirLister()->url());
        delete data;

        // Clear system clipboard.
        if (mimeData->hasFormat("application/x-cutefish-cutselection")) {
            QApplication::clipboard()->clear();
        }
    }
}

void FolderModel::cut()
{
    if (!m_selectionModel->hasSelection())
        return;

    if (QAction *action = m_actions.value("cut"))
        if (!action->isEnabled())
            return;

    QMimeData *mimeData = FolderContentsModel::mimeData(m_selectionModel->selectedIndexes());

    mimeData->setData("application/x-cutefish-cutselection", QByteArray("1"));

    QApplication::clipboard()->setMimeData(mimeData);
}

void FolderModel::openSelected()
{
    if (!m_selectionModel->hasSelection())
        return;

    if (resolvedUrl().scheme() == QLatin1String("trash"))
        return;

    const QList<QUrl> urls = selectedUrls();
    if (!m_isDesktop) {
        if (urls.size() == 1 && Cutefish::IO::FileItem(urls.first()).isDir()) {
            setUrl(urls.first().toLocalFile());
            return;
        }
    }

    if (urls.size() == 1 && isZipFile(Cutefish::IO::FileItem(urls.first()))) {
        extractSelectedArchive();
        return;
    }

    for (const QUrl &url : urls) {
        Cutefish::IO::FileItem item(url);
        if (m_isDesktop && item.isDir()) {
            openInNewWindow(url.toString());
            continue;
        }

        QString mimeType = item.mimetype();

        // Desktop file.
        if (mimeType == "application/x-desktop") {
            FileLauncher::self()->launchApp(url.toLocalFile(), "");
            continue;
        }

        // runnable
        if (mimeType == "application/x-executable" || mimeType == "application/x-sharedlib"
            || mimeType == "application/x-iso9660-appimage"
            || mimeType == "application/vnd.appimage") {
            QFileInfo fileInfo(url.toLocalFile());
            if (!fileInfo.isExecutable()) {
                QFile file(url.toLocalFile());
                file.setPermissions(file.permissions() | QFile::ExeOwner | QFile::ExeUser
                                    | QFile::ExeGroup | QFile::ExeOther);
            }

            FileLauncher::self()->launchExecutable(url.toLocalFile());

            continue;
        }

        QString defaultAppDesktopFile =
                m_mimeAppManager->getDefaultAppByMimeType(item.currentMimeType());

        // If no default application is found,
        // look for the first one of the frequently used applications.
        if (defaultAppDesktopFile.isEmpty()) {
            QStringList recommendApps =
                    m_mimeAppManager->getRecommendedAppsByMimeType(item.currentMimeType());
            if (recommendApps.count() > 0) {
                defaultAppDesktopFile = recommendApps.first();
            }
        }

        if (!defaultAppDesktopFile.isEmpty()) {
            FileLauncher::self()->launchApp(defaultAppDesktopFile, url.toLocalFile());
            continue;
        }

        QDesktopServices::openUrl(url);
    }
}

void FolderModel::startArchiveJob(OperationTask *job, const QString &operation,
                                  const QString &outputPath)
{
    if (!job)
        return;

    m_archiveJob = job;

    m_archiveBusy = true;
    emit archiveBusyChanged();

    m_archiveCancelling = false;
    emit archiveCancellingChanged();

    m_archiveProgress = -1;
    emit archiveProgressChanged();

    m_archiveOperation = operation;
    emit archiveOperationChanged();

    m_archiveOutputPath = outputPath;

    m_archiveCurrentFile.clear();
    emit archiveCurrentFileChanged();

    connect(job, &OperationTask::progressInfoChanged, this, [this, job] {
        const QVariantMap info = job->progressInfo();
        const int progress = info.value(QStringLiteral("totalsKnown")).toBool()
                ? info.value(QStringLiteral("processedItems")).toInt()
                : -1;
        if (m_archiveProgress != progress) {
            m_archiveProgress = progress;
            emit archiveProgressChanged();
        }
        const QString current = info.value(QStringLiteral("currentPath")).toString();
        if (m_archiveCurrentFile != current) {
            m_archiveCurrentFile = current;
            emit archiveCurrentFileChanged();
        }
    });
    connect(job, &OperationTask::result, this, [this, job](OperationTask *) {
        archiveJobFinished(!job->error() && !job->wasCanceled(), job->wasCanceled(),
                           job->errorString(), m_archiveOutputPath);
        if (m_archiveJob == job)
            m_archiveJob = nullptr;
    });
}

void FolderModel::compressSelected()
{
    if (m_archiveJob || !m_selectionModel->hasSelection())
        return;

    if (resolvedUrl().scheme() == QLatin1String("trash") || !rootItem().isWritable())
        return;

    const QList<QUrl> urls = selectedUrls();
    QStringList paths;
    paths.reserve(urls.size());

    for (const QUrl &url : urls) {
        if (!url.isLocalFile() || url.toLocalFile().isEmpty()) {
            emit notification(tr("Only local files can be compressed."));
            return;
        }
        paths.append(url.toLocalFile());
    }

    const QString destinationDirectory = resolvedUrl().toLocalFile();
    if (destinationDirectory.isEmpty())
        return;

    const QString baseName =
            urls.size() == 1 ? QFileInfo(paths.first()).fileName() : QStringLiteral("Archive");
    const QString outputPath =
            uniquePath(destinationDirectory, baseName + QStringLiteral(".zip"), false);

    startArchiveJob(FileOperationsClient::instance()->compress(paths, outputPath),
                    tr("Compressing"), outputPath);
}

void FolderModel::extractSelectedArchive()
{
    if (m_archiveJob || m_selectionModel->selectedIndexes().size() != 1)
        return;

    const QUrl archiveUrl = selectedUrls().first();
    const Cutefish::IO::FileItem item(archiveUrl);
    if (!isZipFile(item))
        return;

    const QString archivePath = archiveUrl.toLocalFile();
    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.isFile())
        return;

    QString baseName = archiveInfo.completeBaseName();
    if (baseName.isEmpty())
        baseName = QStringLiteral("Extracted");

    const QString outputPath = uniquePath(archiveInfo.absolutePath(), baseName, true);
    startArchiveJob(FileOperationsClient::instance()->extract(archivePath, outputPath),
                    tr("Extracting"), outputPath);
}

void FolderModel::cancelArchive()
{
    if (!m_archiveJob || !m_archiveBusy || m_archiveCancelling)
        return;

    m_archiveCancelling = true;
    emit archiveCancellingChanged();
    m_archiveJob->cancel();
}

void FolderModel::archiveJobFinished(bool success, bool canceled, const QString &error,
                                     const QString &outputPath)
{
    if (!m_archiveBusy)
        return;

    const QString operation = m_archiveOperation;

    m_archiveBusy = false;
    emit archiveBusyChanged();

    m_archiveCancelling = false;
    emit archiveCancellingChanged();

    m_archiveProgress = success ? 100 : -1;
    emit archiveProgressChanged();

    m_archiveCurrentFile.clear();
    emit archiveCurrentFileChanged();

    QString message;
    if (success) {
        m_needSelectUrls.append(QUrl::fromLocalFile(outputPath));
        refresh();
        delayUpdateNeedSelectUrls();
    } else if (canceled) {
        message = tr("%1 canceled.").arg(operation);
        emit notification(message);
    } else {
        message = tr("%1 failed: %2").arg(operation, error);
        // Errors are important enough to leave the operation surface and go
        // to the system notification center. Fall back to the existing
        // in-window toast if no notification service is available.
        if (!sendSystemNotification(tr("Files"), message))
            emit notification(message);
    }
}

void FolderModel::showOpenWithDialog()
{
    if (!m_selectionModel->hasSelection())
        return;

    const QList<QUrl> urls = selectedUrls();

    OpenWithDialog *dlg = new OpenWithDialog(urls.first());
    dlg->show();
}

void FolderModel::deleteSelected()
{
    if (!m_selectionModel->hasSelection()) {
        return;
    }

    if (QAction *action = m_actions.value(QStringLiteral("del"))) {
        if (!action->isEnabled()) {
            return;
        }
    }

    FileOperationsClient::instance()->remove(selectedUrls());
}

void FolderModel::moveSelectedToTrash()
{
    if (!m_selectionModel->hasSelection()) {
        return;
    }

    if (QAction *action = m_actions.value(QStringLiteral("trash"))) {
        if (!action->isEnabled()) {
            return;
        }
    }

    const QList<QUrl> urls = selectedUrls();
    FileOperationsClient::instance()->trash(urls);
}

void FolderModel::emptyTrash()
{
    QProcess::startDetached("cutefish-filemanager", QStringList() << "-e");
}

void FolderModel::keyDeletePress()
{
    if (!m_selectionModel->hasSelection())
        return;

    resolvedUrl().scheme() == "trash" ? openDeleteDialog() : moveSelectedToTrash();
}

void FolderModel::setWallpaperSelected()
{
    if (!m_selectionModel)
        return;

    QUrl url = selectedUrls().first();

    if (!url.isLocalFile())
        return;

    QDBusInterface iface("com.cutefish.Services", "/com/cutefish/Services/Appearance",
                         "com.cutefish.Services.Appearance", QDBusConnection::sessionBus(),
                         nullptr);
    if (iface.isValid())
        iface.call("setWallpaper", url.toLocalFile());
}

void FolderModel::prepareContextMenu()
{
    updateActions();
}

void FolderModel::openContextMenu(QQuickItem *visualParent, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);

    updateActions();

    const QModelIndexList indexes = m_selectionModel->selectedIndexes();
    const bool isTrash = (resolvedUrl().scheme() == QLatin1String("trash"));
    QMenu *menu = new QMenu;

    // Open folder menu.
    if (indexes.isEmpty()) {
        QAction *selectAll = new QAction(tr("Select All"), this);
        connect(selectAll, &QAction::triggered, this, &FolderModel::selectAll);

        menu->addAction(m_actions.value("newFolder"));

        if (!isTrash) {
            QMenu *newMenu = new QMenu(tr("New Documents"));
            newMenu->addAction(m_actions.value("newTextFile"));
            menu->addMenu(newMenu);
        }

        menu->addSeparator();
        menu->addAction(m_actions.value("paste"));
        menu->addAction(selectAll);
        if (m_actions.value("terminal")->isVisible()) {
            menu->addSeparator();
            menu->addAction(m_actions.value("terminal"));
        }

        if (m_isDesktop) {
            menu->addAction(m_actions.value("changeBackground"));
        }

        menu->addSeparator();
        menu->addAction(m_actions.value("showHidden"));

        menu->addSeparator();
        menu->addAction(m_actions.value("emptyTrash"));
        menu->addAction(m_actions.value("properties"));
    } else {
        // Open the items menu.

        // Trash items
        menu->addAction(m_actions.value("restore"));

        menu->addAction(m_actions.value("open"));
        menu->addAction(m_actions.value("openInNewTab"));
        menu->addAction(m_actions.value("openInNewWindow"));
        menu->addAction(m_actions.value("openWith"));
        menu->addAction(m_actions.value("compress"));
        menu->addSeparator();
        menu->addAction(m_actions.value("cut"));
        menu->addAction(m_actions.value("copy"));
        menu->addAction(m_actions.value("trash"));
        menu->addAction(m_actions.value("del"));
        menu->addAction(m_actions.value("rename"));

        if (m_actions.value("terminal")->isVisible()) {
            menu->addSeparator();
            menu->addAction(m_actions.value("terminal"));
        }

        menu->addAction(m_actions.value("wallpaper"));
        menu->addSeparator();
        menu->addAction(m_actions.value("properties"));
    }

    QPoint position;
    if (visualParent) {
        position = visualParent->mapToGlobal(QPointF(0, visualParent->height())).toPoint();
    } else {
        position = QCursor::pos();
    }

    menu->installEventFilter(this);
    menu->setAttribute(Qt::WA_TranslucentBackground);

    m_contextMenu = menu;
    menu->popup(position);

    connect(menu, &QMenu::aboutToHide, [menu]() {
        menu->deleteLater();
    });
}

void FolderModel::openPropertiesDialog()
{
    const QModelIndexList indexes = m_selectionModel->selectedIndexes();

    if (indexes.isEmpty()) {
        FilePropertiesDialog *dlg = new FilePropertiesDialog(QUrl::fromLocalFile(url()));
        dlg->show();
        return;
    }

    Cutefish::IO::FileItemList items;
    items.reserve(indexes.count());
    for (const QModelIndex &index : indexes) {
        Cutefish::IO::FileItem item = itemForIndex(index);
        if (!item.isNull()) {
            items.append(item);
        }
    }

    FilePropertiesDialog *dlg = new FilePropertiesDialog(items);
    dlg->show();
}

void FolderModel::openInTerminal()
{
    QString url;
    if (m_selectionModel->hasSelection()) {
        Cutefish::IO::FileItem item = itemForIndex(m_selectionModel->selectedIndexes().first());
        if (item.isDir()) {
            url = item.url().toLocalFile();
        }
    } else {
        url = rootItem().url().toLocalFile();
    }

    m_mimeAppManager->launchTerminal(url);
}

void FolderModel::openChangeWallpaperDialog()
{
    QProcess::startDetached("cutefish-settings", QStringList() << "-m" << "background");
}

void FolderModel::openDeleteDialog()
{
    Window *w = new Window;
    w->rootContext()->setContextProperty("model", this);
    w->load(QUrl("qrc:/qml/Dialogs/DeleteDialog.qml"));
}

void FolderModel::openInNewWindow(const QString &url)
{
    if (!url.isEmpty()) {
        // Callers pass either a local path or a folder URL.
        const QUrl target(url);
        QProcess::startDetached("cutefish-filemanager",
                                QStringList() << (target.isLocalFile() ? target.toLocalFile() : url));
        return;
    }

    // url 为空则打开已选择的 items.
    if (!m_selectionModel->hasSelection())
        return;

    for (const QModelIndex &index : m_selectionModel->selectedIndexes()) {
        Cutefish::IO::FileItem item = itemForIndex(index);
        if (item.isDir()) {
            QProcess::startDetached("cutefish-filemanager",
                                    QStringList() << item.url().toLocalFile());
        }
    }
}

void FolderModel::openInNewTab()
{
    const QModelIndexList indexes = m_selectionModel->selectedIndexes();
    for (const QModelIndex &index : indexes) {
        const Cutefish::IO::FileItem item = itemForIndex(index);
        if (item.isDir())
            emit openTabRequested(item.url().toString());
    }
}

void FolderModel::restoreFromTrash()
{
    if (!m_selectionModel->hasSelection())
        return;

    if (QAction *action = m_actions.value("restore"))
        if (!action->isVisible())
            return;

    FileOperationsClient::instance()->restore(selectedUrls());
}

void FolderModel::createActions()
{
    QAction *open = new QAction(tr("Open"), this);
    connect(open, &QAction::triggered, this, &FolderModel::openSelected);

    QAction *openWith = new QAction(tr("Open with"), this);
    connect(openWith, &QAction::triggered, this, &FolderModel::showOpenWithDialog);

    QAction *compress = new QAction(tr("Compress"), this);
    connect(compress, &QAction::triggered, this, &FolderModel::compressSelected);

    QAction *cut = new QAction(tr("Cut"), this);
    connect(cut, &QAction::triggered, this, &FolderModel::cut);

    QAction *copy = new QAction(tr("Copy"), this);
    connect(copy, &QAction::triggered, this, &FolderModel::copy);

    QAction *paste = new QAction(tr("Paste"), this);
    connect(paste, &QAction::triggered, this, &FolderModel::paste);

    QAction *newFolder = new QAction(tr("New Folder"), this);
    connect(newFolder, &QAction::triggered, this, &FolderModel::newFolder);

    QMenu *newDocuments = new QMenu(tr("New Documents"));
    QAction *newTextFile = new QAction(tr("New Text"), this);
    connect(newTextFile, &QAction::triggered, this, &FolderModel::newTextFile);
    newDocuments->addAction(newTextFile);

    QAction *trash = new QAction(tr("Move To Trash"), this);
    connect(trash, &QAction::triggered, this, &FolderModel::moveSelectedToTrash);

    QAction *emptyTrash = new QAction(tr("Empty Trash"), this);
    connect(emptyTrash, &QAction::triggered, this, &FolderModel::emptyTrash);

    QAction *del = new QAction(tr("Delete"), this);
    connect(del, &QAction::triggered, this, &FolderModel::openDeleteDialog);

    QAction *rename = new QAction(tr("Rename"), this);
    connect(rename, &QAction::triggered, this, &FolderModel::requestRename);

    QAction *terminal = new QAction(tr("Open in Terminal"), this);
    connect(terminal, &QAction::triggered, this, &FolderModel::openInTerminal);

    QAction *wallpaper = new QAction(tr("Set as Wallpaper"), this);
    connect(wallpaper, &QAction::triggered, this, &FolderModel::setWallpaperSelected);

    QAction *properties = new QAction(tr("Properties"), this);
    QObject::connect(properties, &QAction::triggered, this, &FolderModel::openPropertiesDialog);

    QAction *changeBackground = new QAction(tr("Change background"), this);
    QObject::connect(changeBackground, &QAction::triggered, this,
                     &FolderModel::openChangeWallpaperDialog);

    QAction *restore = new QAction(tr("Restore"), this);
    QObject::connect(restore, &QAction::triggered, this, &FolderModel::restoreFromTrash);

    QAction *showHidden = new QAction(tr("Show Hidden Files"), this);
    QObject::connect(showHidden, &QAction::triggered, this, [this] {
        setShowHiddenFiles(!m_showHiddenFiles);
    });

    QAction *openInNewWindow = new QAction(tr("Open In New Window"), this);
    QObject::connect(openInNewWindow, &QAction::triggered, this, [this] {
        this->openInNewWindow();
    });
    QAction *undo = new QAction(tr("Undo"), this);
    connect(undo, &QAction::triggered, this, &FolderModel::undo);

    QAction *redo = new QAction(tr("Redo"), this);
    connect(redo, &QAction::triggered, this, &FolderModel::redo);

    QAction *openInNewTab = new QAction(tr("Open In New Tab"), this);
    connect(openInNewTab, &QAction::triggered, this, &FolderModel::openInNewTab);
    m_actions.insert(QStringLiteral("openInNewTab"), openInNewTab);
    m_actions.insert(QStringLiteral("undo"), undo);
    m_actions.insert(QStringLiteral("redo"), redo);

    m_actions.insert(QStringLiteral("open"), open);
    m_actions.insert(QStringLiteral("openWith"), openWith);
    m_actions.insert(QStringLiteral("compress"), compress);
    m_actions.insert(QStringLiteral("cut"), cut);
    m_actions.insert(QStringLiteral("copy"), copy);
    m_actions.insert(QStringLiteral("paste"), paste);
    m_actions.insert(QStringLiteral("newFolder"), newFolder);
    m_actions.insert(QStringLiteral("newTextFile"), newTextFile);
    m_actions.insert(QStringLiteral("trash"), trash);
    m_actions.insert(QStringLiteral("emptyTrash"), emptyTrash);
    m_actions.insert(QStringLiteral("del"), del);
    m_actions.insert(QStringLiteral("rename"), rename);
    m_actions.insert(QStringLiteral("terminal"), terminal);
    m_actions.insert(QStringLiteral("wallpaper"), wallpaper);
    m_actions.insert(QStringLiteral("properties"), properties);
    m_actions.insert(QStringLiteral("changeBackground"), changeBackground);
    m_actions.insert(QStringLiteral("restore"), restore);
    m_actions.insert(QStringLiteral("showHidden"), showHidden);
    m_actions.insert(QStringLiteral("openInNewWindow"), openInNewWindow);
}

void FolderModel::updateActions()
{
    const QModelIndexList indexes = m_selectionModel->selectedIndexes();

    Cutefish::IO::FileItemList items;
    QList<QUrl> urls;
    bool hasRemoteFiles = false;
    bool isTrashLink = false;
    bool hasDir = false;
    bool hasStandardFolder = false;
    const bool isTrash = (resolvedUrl().scheme() == QLatin1String("trash"));

    if (indexes.isEmpty()) {
        items << rootItem();
    } else {
        items.reserve(indexes.count());
        urls.reserve(indexes.count());
        for (const QModelIndex &index : indexes) {
            Cutefish::IO::FileItem item = itemForIndex(index);
            if (!item.isNull()) {
                hasRemoteFiles |= item.localPath().isEmpty();
                items.append(item);
                urls.append(item.url());
            }

            if (item.isDir())
                hasDir = true;
            hasStandardFolder |= Cutefish::IO::isStandardFolder(item.url());
        }
    }

    const bool canMoveItems = !items.isEmpty() && rootItem().isWritable();
    // Check if we're showing the menu for the trash link
    if (items.count() == 1 && items.at(0).isDesktopFile()) {
        isTrashLink = items.at(0).targetUrl() == QUrl(QStringLiteral("trash:/"));
    }

    if (QAction *openAction = m_actions.value(QStringLiteral("open"))) {
        openAction->setVisible(!isTrash && !hasDir);
    }

    // The context menu only lists these with a selection; the desktop menu
    // bar lists them always.
    if (QAction *copyAction = m_actions.value(QStringLiteral("copy"))) {
        copyAction->setVisible(!isTrash);
        copyAction->setEnabled(!indexes.isEmpty());
    }

    if (QAction *cutAction = m_actions.value(QStringLiteral("cut"))) {
        cutAction->setVisible(!isTrash);
        cutAction->setEnabled(!indexes.isEmpty() && rootItem().isWritable() && !hasStandardFolder);
    }

    if (QAction *restoreAction = m_actions.value(QStringLiteral("restore"))) {
        restoreAction->setVisible(items.count() >= 1 && isTrash);
    }

    if (QAction *openWith = m_actions.value(QStringLiteral("openWith"))) {
        openWith->setVisible(items.count() == 1 && !isTrash && !hasDir);
    }

    if (QAction *compress = m_actions.value(QStringLiteral("compress"))) {
        const bool canCompress =
                !indexes.isEmpty() && !isTrash && !hasRemoteFiles && rootItem().isWritable();
        compress->setVisible(canCompress);
        compress->setEnabled(canCompress && !m_archiveJob);

        if (indexes.count() == 1 && !items.isEmpty()) {
            compress->setText(tr("Compress “%1”").arg(items.first().url().fileName()));
        } else {
            compress->setText(tr("Compress Items"));
        }
    }

    if (QAction *newFolder = m_actions.value(QStringLiteral("newFolder"))) {
        newFolder->setVisible(!isTrash);
        newFolder->setEnabled(rootItem().isWritable());
    }

    if (QAction *newTextFile = m_actions.value(QStringLiteral("newTextFile"))) {
        newTextFile->setVisible(!isTrash);
        newTextFile->setEnabled(rootItem().isWritable());
    }

    if (QAction *paste = m_actions.value(QStringLiteral("paste"))) {
        bool enable = false;

        const QMimeData *mimeData = QApplication::clipboard()->mimeData();
        QList<QUrl> urls = mimeData ? mimeData->urls() : QList<QUrl>();

        if (!urls.isEmpty()) {
            if (!rootItem().isNull()) {
                enable = rootItem().isWritable();
            }
        }

        paste->setEnabled(enable);
        paste->setVisible(!isTrash);
    }

    if (QAction *rename = m_actions.value(QStringLiteral("rename"))) {
        rename->setEnabled(canMoveItems);
        rename->setVisible(!isTrash && !Fm::isFixedFolder(items.first().url()));
    }

    if (QAction *trash = m_actions.value("trash")) {
        trash->setEnabled(canMoveItems && !hasStandardFolder);
        trash->setVisible(!isTrash);
    }

    if (QAction *emptyTrash = m_actions.value("emptyTrash")) {
        emptyTrash->setVisible(isTrash && rowCount() > 0);
    }

    if (QAction *del = m_actions.value(QStringLiteral("del"))) {
        del->setVisible(isTrash && canMoveItems);
    }

    if (QAction *terminal = m_actions.value("terminal")) {
        terminal->setVisible(items.size() == 1 && items.first().isDir() && !isTrash);
    }

    if (QAction *terminal = m_actions.value("wallpaper")) {
        terminal->setVisible(items.size() == 1 && !isTrash
                             && supportSetAsWallpaper(items.first().mimetype()));
    }

    if (QAction *properties = m_actions.value("properties")) {
        properties->setVisible(!isTrash);
    }

    if (QAction *showHidden = m_actions.value("showHidden")) {
        showHidden->setVisible(!isTrash);
        // Plain text instead of a check mark: the label says what the item does.
        showHidden->setText(m_showHiddenFiles ? tr("Hide Hidden Files") : tr("Show Hidden Files"));
    }

    if (QAction *openInNewWindow = m_actions.value("openInNewWindow")) {
        openInNewWindow->setVisible(hasDir && !isTrash);
    }
    if (QAction *openInNewTab = m_actions.value("openInNewTab")) {
        openInNewTab->setVisible(hasDir && !isTrash && !m_isDesktop);
    }

    if (QAction *undo = m_actions.value("undo")) {
        undo->setEnabled(FileOperationsClient::instance()->undoAvailable());
    }

    if (QAction *redo = m_actions.value("redo")) {
        redo->setEnabled(FileOperationsClient::instance()->redoAvailable());
    }
}
