// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldermodel.h"
#include "operations/fileoperationsclient.h"
#include "dirlister.h"
#include "foldersizecache.h"
#include "helper/datehelper.h"
#include "helper/fm.h"
#include "helper/folderpreferences.h"
#include "helper/iojobui.h"

#include <filesystemurl.h>
#include <thumbnailcache.h>
#include <thumbnailprovider.h>

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QMimeDatabase>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTimeZone>

FolderModel::FolderModel(QObject *parent)
    : FolderContentsModel(parent)
    , m_status(None)
    , m_sortMode(0)
    , m_sortDesc(false)
    , m_sortDirsFirst(true)
    , m_showHiddenFiles(false)
    , m_complete(false)
    , m_isDesktop(false)
    , m_suffixVisible(true)
    , m_dragInProgress(false)
    , m_dragWaitingForImages(false)
    , m_dragImageTimeout(new QTimer(this))
    , m_dropTargetPositionsCleanup(new QTimer(this))
    , m_viewAdapter(nullptr)
    , m_mimeAppManager(MimeAppManager::self())
    , m_archiveJob(nullptr)
    , m_archiveBusy(false)
    , m_archiveCancelling(false)
    , m_archiveProgress(-1)
    , m_archiveOperation()
    , m_archiveCurrentFile()
    , m_currentIndex(-1)
    , m_updateNeedSelectTimer(new QTimer(this))
{
    CutefishFM::installIoJobUi();

    QSettings settings(FolderPreferences::file(), QSettings::IniFormat);
    settings.beginGroup(FolderPreferences::group());
    m_showHiddenFiles = settings.value("showHiddenFiles", false).toBool();

    m_dragImageTimeout->setSingleShot(true);
    m_dragImageTimeout->setInterval(200);
    connect(m_dragImageTimeout, &QTimer::timeout, this, &FolderModel::startPendingDrag);

    m_updateNeedSelectTimer->setSingleShot(true);
    m_updateNeedSelectTimer->setInterval(50);
    connect(m_updateNeedSelectTimer, &QTimer::timeout, this, &FolderModel::updateNeedSelectUrls);

    m_dropTargetPositionsCleanup->setSingleShot(true);
    m_dropTargetPositionsCleanup->setInterval(10000);
    connect(m_dropTargetPositionsCleanup, &QTimer::timeout, this, [this] {
        m_dropTargetPositions.clear();
        m_newDocumentCells.clear();
    });

    m_dirLister = new DirLister(this);
    m_dirLister->setAutoUpdate(true);
    m_dirLister->setShowHiddenFiles(m_showHiddenFiles);

    m_folderSizes = new FolderSizeCache(this);
    connect(m_dirLister, &Cutefish::IO::DirectoryLister::itemsChanged,
            m_folderSizes, &FolderSizeCache::clear);
    connect(m_dirLister, &Cutefish::IO::DirectoryLister::directoryChanged,
            m_folderSizes, &FolderSizeCache::clear);
    connect(m_folderSizes, &FolderSizeCache::ready, this, [this](const QString &path) {
        const auto changed = viewIndexForUrl(QUrl::fromLocalFile(path));

        if (changed.isValid())
            emit dataChanged(changed, changed, {FileSizeRole});
    });

    m_dirModel = new Cutefish::IO::DirectoryModel(this);
    m_dirModel->setDirLister(m_dirLister);
    m_selectionModel = new QItemSelectionModel(this, this);
    setSourceModel(m_dirModel);

    connect(m_dirLister, &Cutefish::IO::DirectoryLister::error, this, &FolderModel::notification);
    connect(m_dirLister, &Cutefish::IO::DirectoryLister::directoryChanged, this, [this] {
        if (m_dirLister->isSearching())
            return;

        const auto directory = m_dirLister->url();
        const auto path = directory.toString(QUrl::PreferLocalFile);

        if (path == m_url)
            return;

        m_url = path;
        m_pathHistory.replaceCurrent(directory);
        emit urlChanged();
        emit resolvedUrlChanged();
        emit canGoBackChanged();
        emit canGoForwardChanged();
    });
    connect(m_dirLister, &Cutefish::IO::DirectoryLister::started, this, [this] {
        cancelPreparation();
        setStatus(Listing);
        setSearchBusy(m_dirLister->isSearching());
    });
    connect(m_dirLister, &Cutefish::IO::DirectoryLister::canceled, this, [this] {
        cancelPreparation();
        setStatus(Canceled);
        setSearchBusy(false);
    });
    connect(m_dirLister, &Cutefish::IO::DirectoryLister::completed, this, [this] {
        setSearchBusy(false);
    });
    // Results are not watched; whatever a file operation changed shows once it ends.
    connect(FileOperationsClient::instance(), &FileOperationsClient::activeTaskCountChanged,
            this, [this](int count) {
        if (count == 0 && m_dirLister->isSearching())
            m_dirLister->emitChanges();
    });

    connect(this, &FolderContentsModel::preparationStarted, this, [this] {
        setStatus(Listing);
    });
    connect(this, &FolderContentsModel::preparationFinished, this, [this] {
        setStatus(Ready);
        applyPendingSelection();
        emit listingCompleted();
    });

    connect(m_selectionModel, &QItemSelectionModel::selectionChanged,
            this, &FolderModel::selectionChanged);
    connect(this, &QAbstractItemModel::rowsAboutToBeInserted, this, [this] {
        if (!m_dropTargetPositions.isEmpty())
            setSortMode(-1);
    });
    connect(this, &QAbstractItemModel::rowsInserted, this, &FolderModel::onRowsInserted);
    connect(this, &QAbstractItemModel::rowsInserted, this, &FolderModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &FolderModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &FolderModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, [this] {
        m_currentIndex = -1;
        emit currentIndexChanged();
        selectionChanged({}, {});
    });
    connect(this, &QAbstractItemModel::layoutChanged, this, [this] {
        clearDragImages();
        const auto selected = m_selectionModel->selectedIndexes();
        m_currentIndex = selected.isEmpty() ? -1 : selected.first().row();
        emit currentIndexChanged();
    });

    createActions();
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &FolderModel::updateActions);
    connect(FileOperationsClient::instance(), &FileOperationsClient::historyChanged, this,
            &FolderModel::updateActions);
    connect(this, &FolderModel::listingCompleted, this, &FolderModel::updateActions);
}

FolderModel::~FolderModel()
{
    cancelPreparation();
    clearDragImages();
}

void FolderModel::rebuildContents()
{
    FolderQuery query;
    query.sortColumn = m_sortMode;
    query.descending = m_sortDesc;
    query.directoriesFirst = m_sortDirsFirst;

    setQuery(query);
}

void FolderModel::classBegin()
{
}

void FolderModel::componentComplete()
{
    m_complete = true;
    rebuildContents();
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
    roleNames[IsHiddenRole] = "isHidden";
    roleNames[IsLinkRole] = "isLink";
    roleNames[UrlRole] = "url";
    roleNames[DisplayNameRole] = "displayName";
    roleNames[FileNameRole] = "fileName";
    roleNames[FileSizeRole] = "fileSize";
    roleNames[IconNameRole] = "iconName";
    roleNames[ThumbnailRole] = "thumbnail";
    roleNames[ModifiedRole] = "modified";
    roleNames[IsDesktopFileRole] = "desktopFile";
    roleNames[ModifiedDateRole] = "modifiedDate";
    roleNames[FileTypeRole] = "fileType";
    return roleNames;
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.row() >= count())
        return QVariant();

    Cutefish::IO::FileItem item = itemForIndex(index);

    switch (role) {
    case BlankRole:
        return m_isDesktop && m_dragIndexes.contains(index);
    case SelectedRole:
        return m_selectionModel->isSelected(index);
    case UrlRole:
        return item.url();
    case DisplayNameRole: {
        const QString standard = item.isDir() && item.url().isLocalFile()
                ? Fm::standardFolderName(item.url().toLocalFile())
                : QString();
        return standard.isEmpty() ? item.text() : standard;
    }
    case FileNameRole: {
        return item.url().fileName();
    }
    case IsDesktopFileRole: {
        return item.isDesktopFile();
    }
    case IsDirRole: {
        return item.isDir();
    }
    case IsHiddenRole: {
        return item.isHidden();
    }
    case IsLinkRole: {
        return item.isLink();
    }
    case FileSizeRole: {
        if (item.isDir()) {
            const qint64 count = m_folderSizes->count(item.localPath());
            if (count < 0)
                return QString();
            return count == 1 ? tr("%1 item").arg(count) : tr("%1 items").arg(count);
        }

        return Cutefish::IO::convertSize(item.size());
    }
    case IconNameRole:
        return item.iconName();
    case ThumbnailRole: {
        if (item.isLocalFile()) {
            // The modification time makes the URL differ once the file is
            // replaced in place; an unchanged source is never reloaded by Image.
            QUrl url = item.url();
            url.setQuery(QStringLiteral("mtime=")
                         + QString::number(item.time(Cutefish::IO::FileItem::ModificationTime,
                                                     QTimeZone::UTC)
                                                   .toMSecsSinceEpoch()));

            // Svg Image
            if (item.mimetype() == "image/svg" || item.mimetype() == "image/svg+xml") {
                return url;
            }

            // Support
            if (isSupportThumbnails(item.mimetype())) {
                return "image://thumbnailer/" + url.toString();
            }
        }

        return QVariant();
    }
    case ModifiedRole: {
        return DateHelper::friendlyTime(item.time(Cutefish::IO::FileItem::ModificationTime));
    }
    case ModifiedDateRole: {
        return DateHelper::columnTime(item.time(Cutefish::IO::FileItem::ModificationTime));
    }
    case FileTypeRole: {
        if (item.isDir())
            return tr("Folder");

        const QString comment = item.typeDescription();
        return comment.isEmpty() ? item.mimetype() : comment;
    }
    default:
        break;
    }

    return FolderContentsModel::data(index, role);
}

int FolderModel::indexForKeyboardSearch(const QString &text, int startFromIndex) const
{
    startFromIndex = qMax(0, startFromIndex);

    for (int i = startFromIndex; i < rowCount(); ++i) {
        if (fileItem(i).text().startsWith(text, Qt::CaseInsensitive)) {
            return i;
        }
    }

    for (int i = 0; i < startFromIndex; ++i) {
        if (fileItem(i).text().startsWith(text, Qt::CaseInsensitive)) {
            return i;
        }
    }

    return -1;
}

Cutefish::IO::FileItem FolderModel::itemForIndex(const QModelIndex &index) const
{
    return snapshotItem(index);
}

QModelIndex FolderModel::indexForUrl(const QUrl &url) const
{
    return viewIndexForUrl(url);
}

Cutefish::IO::FileItem FolderModel::fileItem(int index) const
{
    if (index >= 0 && index < count()) {
        return itemForIndex(FolderModel::index(index, 0));
    }

    return Cutefish::IO::FileItem();
}

QVariantMap FolderModel::tabState() const
{
    const auto strings = [](const QList<QUrl> &urls) {
        QStringList list;
        for (const QUrl &url : urls)
            list.append(url.toString());
        return list;
    };

    return {{QStringLiteral("back"), strings(m_pathHistory.backward())},
            {QStringLiteral("forward"), strings(m_pathHistory.forward())},
            {QStringLiteral("selection"), strings(selectedUrls())}};
}

void FolderModel::restoreHistory(const QStringList &back, const QStringList &forward)
{
    const auto urls = [](const QStringList &strings) {
        QList<QUrl> list;
        for (const QString &string : strings)
            list.append(QUrl(string));
        return list;
    };

    m_pathHistory.restore(urls(back), urls(forward));
    emit canGoBackChanged();
    emit canGoForwardChanged();
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

int FolderModel::currentIndex() const
{
    return m_currentIndex;
}

QString FolderModel::url() const
{
    return m_url;
}

void FolderModel::setUrl(const QString &url)
{
    if (url.isEmpty())
        return;

    const bool isTrash = url.startsWith("trash:/");
    const QUrl resolvedNewUrl = isTrash ? QUrl(QStringLiteral("trash:/")) : resolve(url);

    // Opening a folder ends a search; opening the searched one lists it again.
    const bool wasSearching = !m_searchText.isEmpty();
    if (wasSearching) {
        m_searchText.clear();
        emit searchTextChanged();

        if (resolvedNewUrl == resolvedUrl()) {
            updateSearch();
            emit canGoBackChanged();
            return;
        }
    }

    // Refresh this directory.
    if (resolvedNewUrl == resolvedUrl()) {
        refresh();
        emit canGoBackChanged();
        emit canGoForwardChanged();
        return;
    }

    setStatus(Status::Listing);
    m_pendingSelection.clear();

    if (m_pathHistory.isEmpty() || m_pathHistory.last() != resolvedNewUrl) {
        // The results' scroll offset is not the folder's.
        if (!wasSearching)
            leaveFolder();
        m_pathHistory.append(resolvedNewUrl);
    }

    m_url = resolvedNewUrl.toString(QUrl::PreferLocalFile);
    m_dirModel->dirLister()->openUrl(isTrash ? QUrl(QStringLiteral("trash:/")) : resolvedNewUrl);
    clearDragImages();
    m_dragIndexes.clear();

    emit urlChanged();
    emit resolvedUrlChanged();
    emit canGoBackChanged();
    emit canGoForwardChanged();
}

QUrl FolderModel::resolvedUrl() const
{
    return m_dirModel->dirLister()->url();
}

QUrl FolderModel::resolve(const QString &url)
{
    if (url == QLatin1String("~"))
        return QUrl::fromLocalFile(QDir::homePath());

    if (url.startsWith(QLatin1String("~/")))
        return QUrl::fromLocalFile(QDir::home().filePath(url.mid(2)));

    if (QDir::isAbsolutePath(url))
        return QUrl::fromLocalFile(QDir::cleanPath(url));

    const QUrl parsed(url);

    if (!parsed.scheme().isEmpty())
        return parsed;

    return QUrl::fromLocalFile(QDir::current().absoluteFilePath(url));
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
    if (mode < -1 || mode >= Cutefish::IO::DirectoryModel::ColumnCount || m_sortMode == mode)
        return;

    m_sortMode = mode;
    rebuildContents();
    emit sortModeChanged();
}

bool FolderModel::sortDesc() const
{
    return m_sortDesc;
}

void FolderModel::setSortDesc(bool desc)
{
    if (m_sortDesc == desc)
        return;

    m_sortDesc = desc;
    rebuildContents();
    emit sortDescChanged();
}

bool FolderModel::sortDirsFirst() const
{
    return m_sortDirsFirst;
}

void FolderModel::setSortDirsFirst(bool enable)
{
    if (m_sortDirsFirst == enable)
        return;

    m_sortDirsFirst = enable;
    rebuildContents();
    emit sortDirsFirstChanged();
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

bool FolderModel::canGoBack() const
{
    return !m_searchText.isEmpty() || m_pathHistory.canGoBack();
}

bool FolderModel::canGoForward() const
{
    return m_pathHistory.canGoForward();
}

Qt::DropActions FolderModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::DropActions FolderModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Cutefish::IO::FileItem FolderModel::rootItem() const
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

QString FolderModel::standardFolderName(const QString &path) const
{
    return Fm::standardFolderName(path);
}

bool FolderModel::isThumbnailCached(const QUrl &source, const QSizeF &sourceSize, QQuickItem *image) const
{
    if (!image)
        return false;

    // The size QQuickImage asks an image provider for.
    const qreal ratio = image->window() ? image->window()->effectiveDevicePixelRatio() : qApp->devicePixelRatio();
    const QSize requestedSize = sourceSize.toSize() * ratio;
    return ThumbnailProvider::isCached(source, requestedSize);
}

void FolderModel::up()
{
    const QUrl child = resolvedUrl();
    const QUrl &url = Cutefish::IO::upUrl(child);

    if (url.isValid()) {
        setUrl(url.toString());
        selectWhenListed({child});
    }
}

void FolderModel::goBack()
{
    if (!m_searchText.isEmpty()) {
        setSearchText(QString());
        return;
    }

    if (!m_pathHistory.canGoBack())
        return;

    leaveFolder();
    setUrl(m_pathHistory.previousPath().toString());
    selectWhenListed(m_pathHistory.currentSelection());
}

void FolderModel::goForward()
{
    if (!m_pathHistory.canGoForward())
        return;

    if (m_searchText.isEmpty())
        leaveFolder();
    setUrl(m_pathHistory.posteriorPath().toString());
    selectWhenListed(m_pathHistory.currentSelection());
}

void FolderModel::leaveFolder()
{
    emit aboutToLeaveFolder();
    m_pathHistory.setCurrentSelection(selectedUrls());
}

qreal FolderModel::viewPosition() const
{
    return m_pathHistory.currentPosition();
}

void FolderModel::setViewPosition(qreal position)
{
    m_pathHistory.setCurrentPosition(position);
}

void FolderModel::refresh()
{
    m_dirModel->dirLister()->updateDirectory(m_dirModel->dirLister()->url());
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

void FolderModel::keyboardSearch(const QString &text)
{
    if (rowCount() == 0)
        return;

    int index;
    int currentIndex = -1;

    if (m_selectionModel->hasSelection()) {
        currentIndex = m_selectionModel->selectedIndexes().first().row();
    }

    index = indexForKeyboardSearch(text, (currentIndex + 1) % rowCount());

    if (index < 0 || currentIndex == index)
        return;

    if (index >= 0) {
        clearSelection();
        setSelected(index);

        emit scrollToItem(index);
    }
}

bool FolderModel::isSupportThumbnails(const QString &mimeType) const
{
    return ThumbnailCache::self()->canThumbnail(mimeType);
}

QString FolderModel::searchText() const
{
    return m_searchText;
}

void FolderModel::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;

    m_searchText = text;
    updateSearch();
    emit searchTextChanged();
    emit canGoBackChanged();
}

bool FolderModel::searchHome() const
{
    return m_searchHome;
}

void FolderModel::setSearchHome(bool home)
{
    if (m_searchHome == home)
        return;

    m_searchHome = home;
    updateSearch();
    emit searchHomeChanged();
}

bool FolderModel::searchBusy() const
{
    return m_searchBusy;
}

QString FolderModel::selectedPath() const
{
    const auto indexes = m_selectionModel->selectedIndexes();

    return indexes.size() == 1 ? itemForIndex(indexes.first()).localPath() : QString();
}

void FolderModel::setSearchBusy(bool busy)
{
    if (m_searchBusy != busy) {
        m_searchBusy = busy;
        emit searchBusyChanged();
    }
}

void FolderModel::updateSearch()
{
    m_pendingSelection.clear();
    clearDragImages();
    m_dragIndexes.clear();

    const QUrl root = m_searchHome ? QUrl::fromLocalFile(QDir::homePath()) : resolvedUrl();
    m_dirLister->search(root, m_searchText);
}

void FolderModel::showInFolder()
{
    const auto indexes = m_selectionModel->selectedIndexes();
    if (indexes.size() != 1)
        return;

    const QUrl url = itemForIndex(indexes.first()).url();
    setUrl(Cutefish::IO::upUrl(url).toString(QUrl::PreferLocalFile));
    selectUrlsWhenListed({url.toString(QUrl::PreferLocalFile)});
}

bool FolderModel::showHiddenFiles() const
{
    return m_showHiddenFiles;
}

void FolderModel::setShowHiddenFiles(bool showHiddenFiles)
{
    if (m_showHiddenFiles != showHiddenFiles) {
        m_showHiddenFiles = showHiddenFiles;

        m_dirLister->setShowHiddenFiles(m_showHiddenFiles);
        m_dirLister->emitChanges();

        QSettings settings(FolderPreferences::file(), QSettings::IniFormat);
        settings.beginGroup(FolderPreferences::group());
        settings.setValue("showHiddenFiles", m_showHiddenFiles);

        emit showHiddenFilesChanged();
    }
}

bool FolderModel::archiveBusy() const
{
    return m_archiveBusy;
}

bool FolderModel::archiveCancelling() const
{
    return m_archiveCancelling;
}

int FolderModel::archiveProgress() const
{
    return m_archiveProgress;
}

QString FolderModel::archiveOperation() const
{
    return m_archiveOperation;
}

QString FolderModel::archiveCurrentFile() const
{
    return m_archiveCurrentFile;
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
