/***************************************************************************
 *   Copyright (C) 2006 David Faure <faure@kde.org>                        *
 *   Copyright (C) 2008 Fredrik Höglund <fredrik@kde.org>                  *
 *   Copyright (C) 2008 Rafael Fernández López <ereslibre@kde.org>         *
 *   Copyright (C) 2011 Marco Martin <mart@kde.org>                        *
 *   Copyright (C) 2014 by Eike Hein <hein@kde.org>                        *
 *   Copyright (C) 2021 Reven Martin <revenmartin@gmail.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "foldermodel.h"
#include "dirlister.h"

#include "../dialogs/filepropertiesdialog.h"
#include "../dialogs/createfolderdialog.h"
#include "../dialogs/openwithdialog.h"

#include "../helper/datehelper.h"
#include "../helper/filelauncher.h"

// Qt
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QCollator>
#include <QDBusInterface>
#include <QStandardPaths>
#include <QApplication>
#include <QDesktopWidget>
#include <QMimeDatabase>
#include <QClipboard>
#include <QPainter>
#include <QDrag>
#include <QDir>
#include <QProcess>
#include <QDesktopServices>

// Qt Quick
#include <QQuickItem>

// KIO
#include <KIO/CopyJob>
#include <KIO/Job>
#include <KIO/PreviewJob>
#include <KIO/DeleteJob>
#include <KIO/DropJob>
#include <KIO/FileUndoManager>
#include <KIO/JobUiDelegate>
#include <KIO/Paste>
#include <KIO/PasteJob>
#include <KIO/RestoreJob>
#include <KUrlMimeData>
#include <KFileItemListProperties>
#include <KDesktopFile>

FolderModel::FolderModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_dirWatch(nullptr)
    , m_sortMode(0)
    , m_sortDesc(false)
    , m_sortDirsFirst(true)
    , m_complete(false)
    , m_isDesktop(false)
    , m_actionCollection(this)
    , m_dragInProgress(false)
    , m_viewAdapter(nullptr)
    , m_mimeAppManager(MimeAppManager::self())
{
    DirLister *dirLister = new DirLister(this);
    dirLister->setDelayedMimeTypes(true);
    dirLister->setAutoErrorHandlingEnabled(false, nullptr);

    m_dirModel = new KDirModel(this);
    m_dirModel->setDirLister(dirLister);
    m_dirModel->setDropsAllowed(KDirModel::DropOnDirectory | KDirModel::DropOnLocalExecutable);
    m_dirModel->moveToThread(qApp->thread());

    m_selectionModel = new QItemSelectionModel(this, this);
    connect(m_selectionModel, &QItemSelectionModel::selectionChanged, this, &FolderModel::selectionChanged);

    setSourceModel(m_dirModel);
    setSortLocaleAware(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);

    sort(m_sortMode, m_sortDesc ? Qt::DescendingOrder : Qt::AscendingOrder);
    createActions();

    connect(this, SIGNAL(rowsInserted(QModelIndex, int, int)), SIGNAL(countChanged()));
    connect(this, SIGNAL(rowsRemoved(QModelIndex, int, int)), SIGNAL(countChanged()));
    connect(this, SIGNAL(modelReset()), SIGNAL(countChanged()));
}

FolderModel::~FolderModel()
{

}

void FolderModel::classBegin()
{

}

void FolderModel::componentComplete()
{
    m_complete = true;
    invalidate();
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return staticRoleNames();
}

QHash<int, QByteArray> FolderModel::staticRoleNames()
{
    QHash<int, QByteArray> roleNames;
    roleNames[Qt::DisplayRole] = "display";
    roleNames[Qt::DecorationRole] = "decoration";
    roleNames[BlankRole] = "blank";
    roleNames[SelectedRole] = "selected";
    roleNames[IsDirRole] = "isDir";
    roleNames[UrlRole] = "url";
    roleNames[DisplayNameRole] = "displayName";
    roleNames[FileNameRole] = "fileName";
    roleNames[FileSizeRole] = "fileSize";
    roleNames[IconNameRole] = "iconName";
    roleNames[ThumbnailRole] = "thumbnail";
    roleNames[ModifiedRole] = "modified";
    roleNames[IsDesktopFileRole] = "desktopFile";
    return roleNames;
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    KFileItem item = itemForIndex(index);

    switch (role) {
    case BlankRole:
        return m_dragIndexes.contains(index);
    case SelectedRole:
        return m_selectionModel->isSelected(index);
    case UrlRole:
        return item.url();
    case DisplayNameRole: {
        if (item.isDesktopFile())
            return "";

        return item.url().fileName();
    }
    case FileNameRole: {
        return item.url().fileName();
    }
    case IsDesktopFileRole: {
        return item.isDesktopFile();
    }
    case FileSizeRole: {
        if (item.isDir()) {
            QDir dir(item.url().toLocalFile());
            dir.setFilter(QDir::Dirs | QDir::AllEntries | QDir::NoDotAndDotDot);
            uint count = dir.count();
            return count == 1 ? tr("%1 item").arg(count) : tr("%1 items").arg(count);
        }

        return KIO::convertSize(item.size());
    }
    case IconNameRole:
        return item.iconName();
    case ThumbnailRole: {
        if (item.isLocalFile()) {
            // Svg Image
            if (item.mimetype() == "image/svg" ||
                    item.mimetype() == "image/svg+xml") {
                return item.url();
            }

            // Support
            if (isSupportThumbnails(item.mimetype())) {
                return "image://thumbnailer/" + item.url().toString();
            }
        }

        return QVariant();
    }
    case ModifiedRole: {
        return DateHelper::friendlyTime(item.time(KFileItem::ModificationTime));
    }
    default:
        break;
    }

    return QSortFilterProxyModel::data(index, role);
}

KFileItem FolderModel::itemForIndex(const QModelIndex &index) const
{
    return m_dirModel->itemForIndex(mapToSource(index));
}

QList<QUrl> FolderModel::selectedUrls() const
{
    const auto indexes = m_selectionModel->selectedIndexes();

    QList<QUrl> urls;
    urls.reserve(indexes.count());

    for (const QModelIndex &index : indexes) {
        urls.append(itemForIndex(index).url());
    }

    return urls;
}

QString FolderModel::url() const
{
    return m_url;
}

void FolderModel::setUrl(const QString &url)
{
    if (url.isEmpty())
        return;

    const QUrl &resolvedNewUrl = resolve(url);

    // Refresh this directory.
    if (url == m_url) {
        m_dirModel->dirLister()->updateDirectory(resolvedNewUrl);
        return;
    }

    m_pathHistory.append(resolvedNewUrl);

    beginResetModel();
    m_url = url;
    m_dirModel->dirLister()->openUrl(resolvedNewUrl);
    clearDragImages();
    m_dragIndexes.clear();
    endResetModel();

    emit urlChanged();
    emit resolvedUrlChanged();

    if (m_dirWatch) {
        delete m_dirWatch;
        m_dirWatch = nullptr;
    }

    if (resolvedNewUrl.isValid()) {
        m_dirWatch = new KDirWatch(this);
        m_dirWatch->addFile(resolvedNewUrl.toLocalFile() + QLatin1String("/.directory"));
    }
}

QUrl FolderModel::resolvedUrl() const
{
    return m_dirModel->dirLister()->url();
}

QUrl FolderModel::resolve(const QString &url)
{
    QUrl resolvedUrl;

    if (url.startsWith(QLatin1Char('~'))) {
        resolvedUrl = QUrl::fromLocalFile(QDir::homePath());
    } else {
        resolvedUrl = QUrl::fromUserInput(url);
    }

    return resolvedUrl;
}

FolderModel::Status FolderModel::status() const
{
    return m_status;
}

void FolderModel::setStatus(FolderModel::Status status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

int FolderModel::sortMode() const
{
    return m_sortMode;
}

void FolderModel::setSortMode(int mode)
{
    if (m_sortMode != mode) {
        m_sortMode = mode;

        if (mode == -1 /* Unsorted */) {
            setDynamicSortFilter(false);
        } else {
            invalidateIfComplete();
            sort(m_sortMode, m_sortDesc ? Qt::DescendingOrder : Qt::AscendingOrder);
            setDynamicSortFilter(true);
        }

        emit sortModeChanged();
    }
}

bool FolderModel::sortDirsFirst() const
{
    return m_sortDirsFirst;
}

void FolderModel::setSortDirsFirst(bool enable)
{
    if (m_sortDirsFirst != enable) {
        m_sortDirsFirst = enable;

        if (m_sortMode != -1 /* Unsorted */) {
            invalidateIfComplete();
            sort(m_sortMode, m_sortDesc ? Qt::DescendingOrder : Qt::AscendingOrder);
        }

        emit sortDirsFirstChanged();
    }
}

QObject *FolderModel::viewAdapter() const
{
    return m_viewAdapter;
}

void FolderModel::setViewAdapter(QObject *adapter)
{
    if (m_viewAdapter != adapter) {
        ItemViewAdapter *viewAdapter = dynamic_cast<ItemViewAdapter *>(adapter);
        m_viewAdapter = viewAdapter;
        emit viewAdapterChanged();
    }
}

bool FolderModel::dragging() const
{
    return m_dragInProgress;
}

bool FolderModel::isDir(const QModelIndex &index, const KDirModel *dirModel) const
{
    KFileItem item = dirModel->itemForIndex(index);
    if (item.isDir()) {
        return true;
    }

    return false;
}

bool FolderModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const KDirModel *dirModel = static_cast<KDirModel *>(sourceModel());

    if (m_sortDirsFirst || left.column() == KDirModel::Size) {
        bool leftIsDir = isDir(left, dirModel);
        bool rightIsDir = isDir(right, dirModel);

        if (leftIsDir && !rightIsDir) {
            return (sortOrder() == Qt::AscendingOrder);
        }

        if (!leftIsDir && rightIsDir) {
            return (sortOrder() == Qt::DescendingOrder);
        }
    }

    const KFileItem leftItem = dirModel->data(left, KDirModel::FileItemRole).value<KFileItem>();
    const KFileItem rightItem = dirModel->data(right, KDirModel::FileItemRole).value<KFileItem>();
    const int column = left.column();
    int result = 0;

    switch (column) {
    case KDirModel::Size: {
        if (isDir(left, dirModel) && isDir(right, dirModel)) {
            const int leftChildCount = dirModel->data(left, KDirModel::ChildCountRole).toInt();
            const int rightChildCount = dirModel->data(right, KDirModel::ChildCountRole).toInt();
            if (leftChildCount < rightChildCount)
                result = -1;
            else if (leftChildCount > rightChildCount)
                result = +1;
        } else {
            const KIO::filesize_t leftSize = leftItem.size();
            const KIO::filesize_t rightSize = rightItem.size();
            if (leftSize < rightSize)
                result = -1;
            else if (leftSize > rightSize)
                result = +1;
        }

        break;
    }
    case KDirModel::ModifiedTime: {
        const long long leftTime = leftItem.entry().numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1);
        const long long rightTime = rightItem.entry().numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1);
        if (leftTime < rightTime)
            result = -1;
        else if (leftTime > rightTime)
            result = +1;

        break;
    }
    case KDirModel::Type:
        result = QString::compare(dirModel->data(left, Qt::DisplayRole).toString(), dirModel->data(right, Qt::DisplayRole).toString());
        break;

    default:
        break;
    }

    if (result != 0)
        return result < 0;

    QCollator collator;

    result = collator.compare(leftItem.text(), rightItem.text());

    if (result != 0)
        return result < 0;

    result = collator.compare(leftItem.name(), rightItem.name());

    if (result != 0)
        return result < 0;

    return QString::compare(leftItem.url().url(), rightItem.url().url(), Qt::CaseSensitive);
}

Qt::DropActions FolderModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::DropActions FolderModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

KFileItem FolderModel::rootItem() const
{
    return m_dirModel->dirLister()->rootItem();
}

int FolderModel::count() const
{
    return rowCount();
}

int FolderModel::selectionCount() const
{
    return m_selectionModel->selectedIndexes().size();
}

QString FolderModel::homePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString FolderModel::desktopPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

QAction *FolderModel::action(const QString &name) const
{
    return m_actionCollection.action(name);
}

void FolderModel::up()
{
    const QUrl &url = KIO::upUrl(resolvedUrl());

    if (url.isValid()) {
        setUrl(url.toString());
    }
}

void FolderModel::goBack()
{
    QUrl url = m_pathHistory.previousPath();

    if (url.isEmpty())
        url = resolvedUrl();

    setUrl(url.toString());
}

void FolderModel::goForward()
{
    QUrl url = m_pathHistory.posteriorPath();

    if (url.isEmpty())
        url = resolvedUrl();

    setUrl(url.toString());
}

void FolderModel::refresh()
{
    m_dirModel->dirLister()->updateDirectory(m_dirModel->dirLister()->url());
}

bool FolderModel::supportSetAsWallpaper(const QString &mimeType)
{
    if (mimeType == "image/jpeg" || mimeType == "image/png")
        return true;

    return false;
}

int FolderModel::fileExtensionBoundary(int row)
{
    const QModelIndex idx = index(row, 0);
    const QString &name = data(idx, Qt::DisplayRole).toString();

    int boundary = name.length();

    if (data(idx, IsDirRole).toBool()) {
        return boundary;
    }

    QMimeDatabase db;
    const QString &ext = db.suffixForFileName(name);

    if (ext.isEmpty()) {
        boundary = name.lastIndexOf(QLatin1Char('.'));

        if (boundary < 1) {
            boundary = name.length();
        }
    } else {
        boundary -= ext.length() + 1;
    }

    return boundary;
}

bool FolderModel::hasSelection() const
{
    return m_selectionModel->hasSelection();
}

bool FolderModel::isSelected(int row) const
{
    if (row < 0)
        return false;

    return m_selectionModel->isSelected(index(row, 0));
}

bool FolderModel::isBlank(int row) const
{
    if (row < 0) {
        return true;
    }

    return data(index(row, 0), BlankRole).toBool();
}

void FolderModel::setSelected(int row)
{
    if (row < 0)
        return;

    m_selectionModel->select(index(row, 0), QItemSelectionModel::Select);
}

void FolderModel::selectAll()
{
    setRangeSelected(0, rowCount() - 1);
}

void FolderModel::toggleSelected(int row)
{
    if (row < 0)
        return;

    m_selectionModel->select(index(row, 0), QItemSelectionModel::Toggle);
}

void FolderModel::setRangeSelected(int anchor, int to)
{
    if (anchor < 0 || to < 0) {
        return;
    }

    QItemSelection selection(index(anchor, 0), index(to, 0));
    m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
}

void FolderModel::updateSelection(const QVariantList &rows, bool toggle)
{
    QItemSelection newSelection;

    int iRow = -1;

    foreach (const QVariant &row, rows) {
        iRow = row.toInt();

        if (iRow < 0) {
            return;
        }

        const QModelIndex &idx = index(iRow, 0);
        newSelection.select(idx, idx);
    }

    if (toggle) {
        QItemSelection pinnedSelection = m_pinnedSelection;
        pinnedSelection.merge(newSelection, QItemSelectionModel::Toggle);
        m_selectionModel->select(pinnedSelection, QItemSelectionModel::ClearAndSelect);
    } else {
        m_selectionModel->select(newSelection, QItemSelectionModel::ClearAndSelect);
    }
}

void FolderModel::clearSelection()
{
    if (m_selectionModel->hasSelection())
        m_selectionModel->clear();
}

void FolderModel::pinSelection()
{
    m_pinnedSelection = m_selectionModel->selection();
}

void FolderModel::unpinSelection()
{
    m_pinnedSelection = QItemSelection();
}

void FolderModel::newFolder()
{
    CreateFolderDialog *dlg = new CreateFolderDialog;
    dlg->setPath(rootItem().url().toString());
    dlg->show();
}

void FolderModel::rename(int row, const QString &name)
{
    if (row < 0)
        return;

    QModelIndex idx = index(row, 0);
    m_dirModel->setData(mapToSource(idx), name, Qt::EditRole);
}

void FolderModel::copy()
{
    if (!m_selectionModel->hasSelection())
        return;

    if (QAction *action = m_actionCollection.action("copy"))
        if (!action->isEnabled())
            return;

    QMimeData *mimeData = QSortFilterProxyModel::mimeData(m_selectionModel->selectedIndexes());
    QApplication::clipboard()->setMimeData(mimeData);
}

void FolderModel::paste()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    bool enable = false;

    // Update paste action
    if (QAction *paste = m_actionCollection.action(QStringLiteral("paste"))) {
        QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(mimeData);

        if (!urls.isEmpty()) {
            if (!rootItem().isNull()) {
                enable = rootItem().isWritable();
            }
        }

        paste->setEnabled(enable);
    }

    if (enable) {
        KIO::paste(mimeData, m_dirModel->dirLister()->url());
    }
}

void FolderModel::cut()
{
    if (!m_selectionModel->hasSelection())
        return;

    if (QAction *action = m_actionCollection.action("cut"))
        if (!action->isEnabled())
            return;

    QMimeData *mimeData = QSortFilterProxyModel::mimeData(m_selectionModel->selectedIndexes());
    KIO::setClipboardDataCut(mimeData, true);
    QApplication::clipboard()->setMimeData(mimeData);
}

void FolderModel::openSelected()
{
    if (!m_selectionModel->hasSelection())
        return;

    const QList<QUrl> urls = selectedUrls();
    if (!m_isDesktop) {
        if (urls.size() == 1 && KFileItem(urls.first()).isDir()) {
            setUrl(urls.first().toLocalFile());
            return;
        }
    }

    for (const QUrl &url : urls) {
        KFileItem item(url);
        QString mimeType = item.mimetype();

        // runnable
        if (mimeType == "application/x-executable" ||
            mimeType == "application/x-sharedlib" ||
            mimeType == "application/x-iso9660-appimage" ||
            mimeType == "application/vnd.appimage") {
            QFileInfo fileInfo(url.toLocalFile());
            if (!fileInfo.isExecutable()) {
                QFile file(url.toLocalFile());
                file.setPermissions(file.permissions() | QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
            }

            FileLauncher::self()->launchExecutable(url.toLocalFile());

            continue;
        }

        QString defaultAppDesktopFile = m_mimeAppManager->getDefaultAppByMimeType(item.currentMimeType());

        // If no default application is found,
        // look for the first one of the frequently used applications.
        if (defaultAppDesktopFile.isEmpty()) {
            QStringList recommendApps = m_mimeAppManager->getRecommendedAppsByMimeType(item.currentMimeType());
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

    if (QAction *action = m_actionCollection.action(QStringLiteral("del"))) {
        if (!action->isEnabled()) {
            return;
        }
    }

    const QList<QUrl> urls = selectedUrls();
    KIO::JobUiDelegate uiDelegate;

    if (uiDelegate.askDeleteConfirmation(urls, KIO::JobUiDelegate::Delete, KIO::JobUiDelegate::DefaultConfirmation)) {
        KIO::Job *job = KIO::del(urls);
        job->uiDelegate()->setAutoErrorHandlingEnabled(true);
    }
}

void FolderModel::moveSelectedToTrash()
{
    if (!m_selectionModel->hasSelection()) {
        return;
    }

    if (QAction *action = m_actionCollection.action(QStringLiteral("trash"))) {
        if (!action->isEnabled()) {
            return;
        }
    }

    const QList<QUrl> urls = selectedUrls();
    KIO::JobUiDelegate uiDelegate;

    if (uiDelegate.askDeleteConfirmation(urls, KIO::JobUiDelegate::Trash, KIO::JobUiDelegate::DefaultConfirmation)) {
        KIO::Job *job = KIO::trash(urls);
        job->uiDelegate()->setAutoErrorHandlingEnabled(true);
        KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Trash, urls, QUrl(QStringLiteral("trash:/")), job);
    }
}

void FolderModel::emptyTrash()
{
    QProcess::startDetached("cutefish-filemanager", QStringList() << "-e");
}

void FolderModel::keyDeletePress()
{
    if (!m_selectionModel->hasSelection())
        return;

    resolvedUrl().scheme() == "trash" ? deleteSelected() : moveSelectedToTrash();
}

void FolderModel::setDragHotSpotScrollOffset(int x, int y)
{
    m_dragHotSpotScrollOffset.setX(x);
    m_dragHotSpotScrollOffset.setY(y);
}

void FolderModel::addItemDragImage(int row, int x, int y, int width, int height, const QVariant &image)
{
    if (row < 0)
        return;

    delete m_dragImages.take(row);

    DragImage *dragImage = new DragImage();
    dragImage->row = row;
    dragImage->rect = QRect(x, y, width, height);
    dragImage->image = image.value<QImage>();
    dragImage->blank = false;

    m_dragImages.insert(row, dragImage);
}

void FolderModel::clearDragImages()
{
    qDeleteAll(m_dragImages);
    m_dragImages.clear();
}

void FolderModel::dragSelected(int x, int y)
{
    if (m_dragInProgress)
        return;

    m_dragInProgress = true;
    emit draggingChanged();

    QMetaObject::invokeMethod(this, "dragSelectedInternal", Qt::QueuedConnection, Q_ARG(int, x), Q_ARG(int, y));
}

void FolderModel::setWallpaperSelected()
{
    if (!m_selectionModel)
        return;

    QUrl url = selectedUrls().first();

    if (!url.isLocalFile())
        return;

    QDBusInterface iface("org.cutefish.Settings", "/Theme",
                         "org.cutefish.Theme",
                         QDBusConnection::sessionBus(), nullptr);
    if (iface.isValid())
        iface.call("setWallpaper", url.toLocalFile());
}

void FolderModel::openContextMenu(QQuickItem *visualParent, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);

    updateActions();

    const QModelIndexList indexes = m_selectionModel->selectedIndexes();
    QMenu *menu = new QMenu;

    // Open folder menu.
    if (indexes.isEmpty()) {
        QAction *selectAll = new QAction(tr("Select All"), this);
        connect(selectAll, &QAction::triggered, this, &FolderModel::selectAll);

        menu->addAction(m_actionCollection.action("newFolder"));
        menu->addSeparator();
        menu->addAction(m_actionCollection.action("paste"));
        menu->addAction(selectAll);
        if (m_actionCollection.action("terminal")->isVisible()) {
            menu->addSeparator();
            menu->addAction(m_actionCollection.action("terminal"));
        }

        if (m_isDesktop) {
            menu->addAction(m_actionCollection.action("changeBackground"));
        }

        menu->addSeparator();
        menu->addAction(m_actionCollection.action("emptyTrash"));
        menu->addAction(m_actionCollection.action("properties"));
    } else {
        // Open the items menu.
        menu->addAction(m_actionCollection.action("open"));
        menu->addAction(m_actionCollection.action("openWith"));
        menu->addAction(m_actionCollection.action("cut"));
        menu->addAction(m_actionCollection.action("copy"));
        menu->addAction(m_actionCollection.action("trash"));
        menu->addAction(m_actionCollection.action("del"));
        menu->addAction(m_actionCollection.action("rename"));

        if (m_actionCollection.action("terminal")->isVisible()) {
            menu->addSeparator();
            menu->addAction(m_actionCollection.action("terminal"));
        }

        menu->addAction(m_actionCollection.action("wallpaper"));
        menu->addSeparator();
        menu->addAction(m_actionCollection.action("properties"));
    }

    QPoint position;
    if (visualParent) {
        position = visualParent->mapToGlobal(QPointF(0, visualParent->height())).toPoint();
    } else {
        position = QCursor::pos();
    }

    menu->installEventFilter(this);
    menu->setAttribute(Qt::WA_TranslucentBackground);
    menu->winId();
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

    KFileItemList items;
    items.reserve(indexes.count());
    for (const QModelIndex &index : indexes) {
        KFileItem item = itemForIndex(index);
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
        KFileItem item = itemForIndex(m_selectionModel->selectedIndexes().first());
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

void FolderModel::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QModelIndexList indices = selected.indexes();
    indices.append(deselected.indexes());

    QVector<int> roles;
    roles.append(SelectedRole);

    foreach (const QModelIndex &index, indices) {
        emit dataChanged(index, index, roles);
    }

    if (!m_selectionModel->hasSelection()) {
        clearDragImages();
    } else {
        foreach (const QModelIndex &idx, deselected.indexes()) {
            delete m_dragImages.take(idx.row());
        }
    }

    updateActions();

    emit selectionCountChanged();
}

void FolderModel::dragSelectedInternal(int x, int y)
{
    if (!m_viewAdapter || !m_selectionModel->hasSelection()) {
        m_dragInProgress = false;
        emit draggingChanged();
        return;
    }

    QQuickItem *item = qobject_cast<QQuickItem *>(m_viewAdapter->adapterView());
    QDrag *drag = new QDrag(item);
    addDragImage(drag, x, y);

    m_dragIndexes = m_selectionModel->selectedIndexes();
    std::sort(m_dragIndexes.begin(), m_dragIndexes.end());

    // TODO: Optimize to emit contiguous groups.
    emit dataChanged(m_dragIndexes.first(), m_dragIndexes.last(), QVector<int>() << BlankRole);

    QModelIndexList sourceDragIndexes;
    sourceDragIndexes.reserve(m_dragIndexes.count());
    foreach (const QModelIndex &index, m_dragIndexes) {
        sourceDragIndexes.append(mapToSource(index));
    }

    drag->setMimeData(m_dirModel->mimeData(sourceDragIndexes));

    // Due to spring-loading (aka auto-expand), the URL might change
    // while the drag is in-flight - in that case we don't want to
    // unnecessarily emit dataChanged() for (possibly invalid) indices
    // after it ends.
    const QUrl currentUrl(m_dirModel->dirLister()->url());

    item->grabMouse();
    drag->exec(supportedDragActions());
    item->ungrabMouse();

    m_dragInProgress = false;
    emit draggingChanged();

    if (m_dirModel->dirLister()->url() == currentUrl) {
        const QModelIndex first(m_dragIndexes.first());
        const QModelIndex last(m_dragIndexes.last());
        m_dragIndexes.clear();
        // TODO: Optimize to emit contiguous groups.
        emit dataChanged(first, last, QVector<int>() << BlankRole);
    }
}

bool FolderModel::isSupportThumbnails(const QString &mimeType) const
{
    const QStringList supportsMimetypes = {"image/bmp", "image/png", "image/gif", "image/jpeg", "image/web",
                                           /*"application/pdf", "application/rtf", "application/doc", "application/odf",
                                           "audio/mpeg", "video/mp4"*/};

    if (supportsMimetypes.contains(mimeType))
        return true;

    return false;
}

bool FolderModel::isDesktop() const
{
    return m_isDesktop;
}

void FolderModel::setIsDesktop(bool isDesktop)
{
    if (m_isDesktop == isDesktop)
        return;

    m_isDesktop = isDesktop;
    emit isDesktopChanged();
}

void FolderModel::invalidateIfComplete()
{
    if (!m_complete)
        return;

    invalidate();
}

void FolderModel::invalidateFilterIfComplete()
{
    if (!m_complete)
        return;

    invalidateFilter();
}

void FolderModel::createActions()
{
    QAction *open = new QAction(tr("Open"), this);
    connect(open, &QAction::triggered, this, &FolderModel::openSelected);

    QAction *openWith = new QAction(tr("Open with"), this);
    connect(openWith, &QAction::triggered, this, &FolderModel::showOpenWithDialog);

    QAction *cut = new QAction(tr("Cut"), this);
    connect(cut, &QAction::triggered, this, &FolderModel::cut);

    QAction *copy = new QAction(tr("Copy"), this);
    connect(copy, &QAction::triggered, this, &FolderModel::copy);

    QAction *paste = new QAction(tr("Paste"), this);
    connect(paste, &QAction::triggered, this, &FolderModel::paste);

    QAction *newFolder = new QAction(tr("New Folder"), this);
    connect(newFolder, &QAction::triggered, this, &FolderModel::newFolder);

    QAction *trash = new QAction(tr("Move To Trash"), this);
    connect(trash, &QAction::triggered, this, &FolderModel::moveSelectedToTrash);

    QAction *emptyTrash = new QAction(tr("Empty Trash"), this);
    connect(emptyTrash, &QAction::triggered, this, &FolderModel::emptyTrash);

    QAction *del = new QAction(tr("Delete"), this);
    connect(del, &QAction::triggered, this, &FolderModel::deleteSelected);

    QAction *rename = new QAction(tr("Rename"), this);
    connect(rename, &QAction::triggered, this, &FolderModel::requestRename);

    QAction *terminal = new QAction(tr("Open in Terminal"), this);
    connect(terminal, &QAction::triggered, this, &FolderModel::openInTerminal);

    QAction *wallpaper = new QAction(tr("Set as Wallpaper"), this);
    connect(wallpaper, &QAction::triggered, this, &FolderModel::setWallpaperSelected);

    QAction *properties = new QAction(tr("Properties"), this);
    QObject::connect(properties, &QAction::triggered, this, &FolderModel::openPropertiesDialog);

    QAction *changeBackground = new QAction(tr("Change background"), this);
    QObject::connect(changeBackground, &QAction::triggered, this, &FolderModel::openChangeWallpaperDialog);

    m_actionCollection.addAction(QStringLiteral("open"), open);
    m_actionCollection.addAction(QStringLiteral("openWith"), openWith);
    m_actionCollection.addAction(QStringLiteral("cut"), cut);
    m_actionCollection.addAction(QStringLiteral("copy"), copy);
    m_actionCollection.addAction(QStringLiteral("paste"), paste);
    m_actionCollection.addAction(QStringLiteral("newFolder"), newFolder);
    m_actionCollection.addAction(QStringLiteral("trash"), trash);
    m_actionCollection.addAction(QStringLiteral("emptyTrash"), emptyTrash);
    m_actionCollection.addAction(QStringLiteral("del"), del);
    m_actionCollection.addAction(QStringLiteral("rename"), rename);
    m_actionCollection.addAction(QStringLiteral("terminal"), terminal);
    m_actionCollection.addAction(QStringLiteral("wallpaper"), wallpaper);
    m_actionCollection.addAction(QStringLiteral("properties"), properties);
    m_actionCollection.addAction(QStringLiteral("changeBackground"), changeBackground);
}

void FolderModel::updateActions()
{
    const QModelIndexList indexes = m_selectionModel->selectedIndexes();

    KFileItemList items;
    QList<QUrl> urls;
    bool hasRemoteFiles = false;
    bool isTrashLink = false;
    const bool isTrash = (resolvedUrl().scheme() == QLatin1String("trash"));

    if (indexes.isEmpty()) {
        items << rootItem();
    } else {
        items.reserve(indexes.count());
        urls.reserve(indexes.count());
        for (const QModelIndex &index : indexes) {
            KFileItem item = itemForIndex(index);
            if (!item.isNull()) {
                hasRemoteFiles |= item.localPath().isEmpty();
                items.append(item);
                urls.append(item.url());
            }
        }
    }

    KFileItemListProperties itemProperties(items);
    // Check if we're showing the menu for the trash link
    if (items.count() == 1 && items.at(0).isDesktopFile()) {
        KDesktopFile file(items.at(0).localPath());
        if (file.hasLinkType() && file.readUrl() == QLatin1String("trash:/")) {
            isTrashLink = true;
        }
    }

    if (QAction *openWith = m_actionCollection.action(QStringLiteral("openWith"))) {
        openWith->setVisible(items.count() == 1);
    }

    if (QAction *newFolder = m_actionCollection.action(QStringLiteral("newFolder"))) {
        newFolder->setVisible(!isTrash);
        newFolder->setEnabled(rootItem().isWritable());
    }

    if (QAction *paste = m_actionCollection.action(QStringLiteral("paste"))) {
        bool enable = false;

        const QMimeData *mimeData = QApplication::clipboard()->mimeData();
        QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(mimeData);

        if (!urls.isEmpty()) {
            if (!rootItem().isNull()) {
                enable = rootItem().isWritable();
            }
        }

        paste->setEnabled(enable);
    }

    if (QAction *rename = m_actionCollection.action(QStringLiteral("rename"))) {
        rename->setEnabled(itemProperties.supportsMoving());
        rename->setVisible(!isTrash);
    }

    if (QAction *trash = m_actionCollection.action("trash")) {
        trash->setEnabled(itemProperties.supportsMoving());
        trash->setVisible(!isTrash);
    }

    if (QAction *emptyTrash = m_actionCollection.action("emptyTrash")) {
        emptyTrash->setVisible(isTrash);
    }

    if (QAction *del = m_actionCollection.action(QStringLiteral("del"))) {
        del->setVisible(isTrash && itemProperties.supportsDeleting());
    }

    if (QAction *terminal = m_actionCollection.action("terminal")) {
        terminal->setVisible(items.size() == 1 && items.first().isDir());
    }

    if (QAction *terminal = m_actionCollection.action("wallpaper")) {
        terminal->setVisible(items.size() == 1 && supportSetAsWallpaper(items.first().mimetype()));
    }

    if (QAction *properties = m_actionCollection.action("properties")) {
        properties->setVisible(!isTrash);
    }
}

void FolderModel::addDragImage(QDrag *drag, int x, int y)
{
    if (!drag || m_dragImages.isEmpty())
        return;

    QRegion region;

    foreach (DragImage *image, m_dragImages) {
        image->blank = isBlank(image->row);
        image->rect.translate(-m_dragHotSpotScrollOffset.x(), -m_dragHotSpotScrollOffset.y());
        if (!image->blank && !image->image.isNull()) {
            region = region.united(image->rect);
        }
    }

    QRect rect = region.boundingRect();
    QPoint offset = rect.topLeft();
    rect.translate(-offset.x(), -offset.y());

    QImage dragImage(rect.size(), QImage::Format_RGBA8888);
    dragImage.fill(Qt::transparent);

    QPainter painter(&dragImage);
    QPoint pos;

    foreach (DragImage *image, m_dragImages) {
        if (!image->blank && !image->image.isNull()) {
            pos = image->rect.translated(-offset.x(), -offset.y()).topLeft();
            image->cursorOffset.setX(pos.x() - (x - offset.x()));
            image->cursorOffset.setY(pos.y() - (y - offset.y()));

            painter.drawImage(pos, image->image);
        }

        // FIXME HACK: Operate on copy.
        image->rect.translate(m_dragHotSpotScrollOffset.x(), m_dragHotSpotScrollOffset.y());
    }

    drag->setPixmap(QPixmap::fromImage(dragImage));
    drag->setHotSpot(QPoint(x - offset.x(), y - offset.y()));
}
