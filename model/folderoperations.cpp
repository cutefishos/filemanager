#include "foldermodel.h"
#include "cutefishnotification.h"
#include "helper/dialogs.h"
#include "helper/filelauncher.h"
#include "helper/fm.h"
#include "mimetype/xdgdesktopfile.h"
#include "operations/fileoperationsclient.h"
#include "operations/operationtask.h"
#include "widgets/itemviewadapter.h"
#include "windowmanager.h"

#include <QApplication>
#include <QClipboard>
#include <QDBusInterface>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTextStream>
#include <algorithm>
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

static const QStringList archiveMimeTypes = {
    QStringLiteral("application/gzip"),
    QStringLiteral("application/vnd.rar"),
    QStringLiteral("application/x-7z-compressed"),
    QStringLiteral("application/x-bzip"),
    QStringLiteral("application/x-bzip-compressed-tar"),
    QStringLiteral("application/x-bzip2"),
    QStringLiteral("application/x-bzip2-compressed-tar"),
    QStringLiteral("application/x-compressed-tar"),
    QStringLiteral("application/x-lzma"),
    QStringLiteral("application/x-lzma-compressed-tar"),
    QStringLiteral("application/x-rar"),
    QStringLiteral("application/x-tar"),
    QStringLiteral("application/x-xz"),
    QStringLiteral("application/x-xz-compressed-tar"),
    QStringLiteral("application/x-zip-compressed"),
    QStringLiteral("application/x-zstd-compressed-tar"),
    QStringLiteral("application/zip"),
    QStringLiteral("application/zstd"),
};

static const QStringList folderArchiveSuffixes = {
    QStringLiteral(".tar.gz"), QStringLiteral(".tgz"),
    QStringLiteral(".tar.bz2"), QStringLiteral(".tbz2"), QStringLiteral(".tbz"),
    QStringLiteral(".tar.xz"), QStringLiteral(".txz"),
    QStringLiteral(".tar.zst"), QStringLiteral(".tzst"),
    QStringLiteral(".tar.lzma"),
    QStringLiteral(".tar"),
    QStringLiteral(".zip"),
    QStringLiteral(".7z"),
    QStringLiteral(".rar"),
};

static const QStringList singleFileSuffixes = {
    QStringLiteral(".gz"),
    QStringLiteral(".bz2"),
    QStringLiteral(".xz"),
    QStringLiteral(".zst"),
    QStringLiteral(".lzma"),
};

// The suffix extraction drops: "a.tar.gz" becomes the folder "a", while
// "a.txt.gz" becomes the file "a.txt".
static QString archiveSuffix(const QString &fileName, bool *singleFile)
{
    for (const QString &suffix : folderArchiveSuffixes) {
        if (fileName.endsWith(suffix, Qt::CaseInsensitive)) {
            *singleFile = false;
            return suffix;
        }
    }

    for (const QString &suffix : singleFileSuffixes) {
        if (fileName.endsWith(suffix, Qt::CaseInsensitive)) {
            *singleFile = true;
            return suffix;
        }
    }

    *singleFile = false;
    return QString();
}

static bool isArchive(const Cutefish::IO::FileItem &item)
{
    if (!item.isLocalFile())
        return false;

    if (archiveMimeTypes.contains(item.mimetype()))
        return true;

    bool singleFile = false;
    return !archiveSuffix(item.url().fileName(), &singleFile).isEmpty();
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

    if (urls.size() == 1 && isArchive(Cutefish::IO::FileItem(urls.first()))) {
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

        // Launchers and programs run only once trusted, so a downloaded one
        // does not run on a double-click.
        if (mimeType == "application/x-desktop" || mimeType == "application/x-executable"
            || mimeType == "application/x-sharedlib"
            || mimeType == "application/x-iso9660-appimage"
            || mimeType == "application/vnd.appimage") {
            const QString path = url.toLocalFile();
            if (FileLauncher::isTrusted(path))
                FileLauncher::self()->launchFile(path);
            else
                CutefishFM::Dialogs::confirmLaunch(windowManager(), path, dialogOwner());

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
    if (!isArchive(item))
        return;

    const QString archivePath = archiveUrl.toLocalFile();
    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.isFile())
        return;

    bool singleFile = false;
    const QString suffix = archiveSuffix(archiveInfo.fileName(), &singleFile);
    QString baseName = suffix.isEmpty() ? archiveInfo.completeBaseName()
                                        : archiveInfo.fileName().chopped(suffix.size());
    if (baseName.isEmpty())
        baseName = QStringLiteral("Extracted");

    const QString outputPath = uniquePath(archiveInfo.absolutePath(), baseName, !singleFile);
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
        // to the system notification center, titled with the Files entry.
        auto *systemNotification = new Cutefish::Notification(this);
        systemNotification->setDesktopEntry(QStringLiteral("cutefish-filemanager"));
        systemNotification->setIconName(QStringLiteral("system-file-manager"));
        systemNotification->setBody(message);
        systemNotification->setTimeout(5000);
        systemNotification->setAutoDelete(true);
        // Without a notification service the in-window toast shows it instead.
        connect(systemNotification, &Cutefish::Notification::failed, this, [this, message] {
            emit notification(message);
        });
        systemNotification->show();
    }
}

void FolderModel::showOpenWithDialog()
{
    if (!m_selectionModel->hasSelection())
        return;

    const QList<QUrl> urls = selectedUrls();
    CutefishFM::Dialogs::openWith(windowManager(), urls.first(), dialogOwner());
}

QVariantList FolderModel::openWithApplications() const
{
    const QList<QUrl> urls = selectedUrls();
    if (urls.count() != 1)
        return {};

    return m_mimeAppManager->recommendedApps(urls.first());
}

bool FolderModel::openWithApplication(const QString &desktopFile) const
{
    const QList<QUrl> urls = selectedUrls();
    if (urls.count() != 1 || desktopFile.isEmpty())
        return false;

    const QUrl url = urls.first();
    const QString fileName = url.isLocalFile() ? url.toLocalFile() : url.toString();
    return FileLauncher::self()->launchApp(desktopFile, fileName);
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
    CutefishFM::Dialogs::confirmEmptyTrash(windowManager(), dialogOwner());
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

void FolderModel::openPropertiesDialog()
{
    const QModelIndexList indexes = m_selectionModel->selectedIndexes();

    Cutefish::IO::FileItemList items;
    if (indexes.isEmpty()) {
        items.append(Cutefish::IO::FileItem(resolvedUrl()));
    } else {
        items.reserve(indexes.count());
        for (const QModelIndex &index : indexes) {
            Cutefish::IO::FileItem item = itemForIndex(index);
            if (!item.isNull())
                items.append(item);
        }
    }

    CutefishFM::Dialogs::properties(windowManager(), items, dialogOwner());
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
    CutefishFM::Dialogs::confirmDelete(windowManager(), this, dialogOwner());
}

void FolderModel::openInNewWindow(const QString &url)
{
    if (!url.isEmpty()) {
        // Callers pass either a local path or a folder URL.
        const QUrl target(url);
        CutefishFM::openFolders({target.isLocalFile() ? target.toLocalFile() : url});
        return;
    }

    // Without a URL, open every selected folder.
    QStringList folders;
    for (const QModelIndex &index : m_selectionModel->selectedIndexes()) {
        Cutefish::IO::FileItem item = itemForIndex(index);
        if (item.isDir())
            folders.append(item.url().toLocalFile());
    }
    CutefishFM::openFolders(folders);
}

WindowManager *FolderModel::windowManager() const
{
    // The engine that created this model hosts its dialogs: the file manager's,
    // or cutefish-shell's for the desktop. A model built in C++ has none.
    if (QQmlEngine *engine = qmlEngine(this))
        return WindowManager::ensure(engine);
    return WindowManager::defaultManager();
}

QWindow *FolderModel::dialogOwner() const
{
    // The desktop surface must not be blocked by, or stack, a modal dialog.
    if (m_isDesktop || !m_viewAdapter)
        return nullptr;

    auto *view = qobject_cast<QQuickItem *>(m_viewAdapter->adapterView());
    return view ? view->window() : nullptr;
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

    QAction *showInFolder = new QAction(tr("Show in Enclosing Folder"), this);
    connect(showInFolder, &QAction::triggered, this, &FolderModel::showInFolder);

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

    QAction *newTextFile = new QAction(tr("New Text"), this);
    connect(newTextFile, &QAction::triggered, this, &FolderModel::newTextFile);

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
    m_actions.insert(QStringLiteral("showInFolder"), showInFolder);
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

            if (item.isDir()) {
                hasDir = true;
                hasStandardFolder |= Cutefish::IO::isStandardFolder(item.url());
            }
        }
    }

    const bool searching = m_dirLister->isSearching();
    // Search results come from many folders; each must let its file go.
    const bool canMoveItems = !items.isEmpty()
            && (searching ? std::all_of(items.cbegin(), items.cend(), [](const auto &item) {
                                return QFileInfo(QFileInfo(item.localPath()).absolutePath())
                                        .isWritable();
                            })
                          : rootItem().isWritable());
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
        cutAction->setEnabled(!indexes.isEmpty() && canMoveItems && !hasStandardFolder);
    }

    if (QAction *restoreAction = m_actions.value(QStringLiteral("restore"))) {
        restoreAction->setVisible(items.count() >= 1 && isTrash);
    }

    if (QAction *openWith = m_actions.value(QStringLiteral("openWith"))) {
        openWith->setVisible(items.count() == 1 && !isTrash && !hasDir);
    }

    if (QAction *showInFolder = m_actions.value(QStringLiteral("showInFolder"))) {
        showInFolder->setVisible(searching && indexes.count() == 1);
    }

    if (QAction *compress = m_actions.value(QStringLiteral("compress"))) {
        const bool canCompress =
                !indexes.isEmpty() && !isTrash && !hasRemoteFiles && !searching
                && rootItem().isWritable();
        compress->setVisible(canCompress);
        compress->setEnabled(canCompress && !m_archiveJob);

        if (indexes.count() == 1 && !items.isEmpty()) {
            compress->setText(tr("Compress “%1”").arg(items.first().url().fileName()));
        } else {
            compress->setText(tr("Compress Items"));
        }
    }

    if (QAction *newFolder = m_actions.value(QStringLiteral("newFolder"))) {
        newFolder->setVisible(!isTrash && !searching);
        newFolder->setEnabled(rootItem().isWritable());
    }

    if (QAction *newTextFile = m_actions.value(QStringLiteral("newTextFile"))) {
        newTextFile->setVisible(!isTrash && !searching);
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
        paste->setVisible(!isTrash && !searching);
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
