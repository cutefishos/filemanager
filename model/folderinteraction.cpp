#include "foldermodel.h"
#include "cio/cfilesizejob.h"
#include "operations/fileoperationsclient.h"

#include <QDir>
#include <QDrag>
#include <QGuiApplication>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QQuickItem>

static bool isDropBetweenSharedViews(const QList<QUrl> &urls, const QUrl &folderUrl)
{
    for (const auto &url : urls) {
        if (folderUrl.adjusted(QUrl::StripTrailingSlash)
            != url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash)) {
            return false;
        }
    }
    return true;
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
    m_currentIndex = row;

    emit currentIndexChanged();
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

void FolderModel::updateSelection(const QVariantList &rows, bool toggle, bool extend)
{
    QList<int> sortedRows;
    sortedRows.reserve(rows.size());

    for (const QVariant &row : rows) {
        const int iRow = row.toInt();

        if (iRow < 0) {
            return;
        }

        if (iRow < rowCount()) {
            sortedRows.append(iRow);
        }
    }

    std::sort(sortedRows.begin(), sortedRows.end());

    // One range per run of rows: the selection model diffs range by range, and
    // a rubber band over a large folder sends hundreds of rows per move.
    QItemSelection newSelection;

    for (int first = 0; first < sortedRows.size();) {
        int last = first;

        while (last + 1 < sortedRows.size() && sortedRows.at(last + 1) <= sortedRows.at(last) + 1) {
            ++last;
        }

        newSelection.append(QItemSelectionRange(index(sortedRows.at(first), 0),
                                                index(sortedRows.at(last), 0)));
        first = last + 1;
    }

    if (toggle || extend) {
        QItemSelection pinnedSelection = m_pinnedSelection;
        pinnedSelection.merge(newSelection, toggle ? QItemSelectionModel::Toggle
                                                   : QItemSelectionModel::Select);
        m_selectionModel->select(pinnedSelection, QItemSelectionModel::ClearAndSelect);
    } else {
        m_selectionModel->select(newSelection, QItemSelectionModel::ClearAndSelect);
    }
}

void FolderModel::clearSelection()
{
    // Keep the selection while our own context menu is open: the menu is a
    // separate window, so showing it makes the view lose focus, and the desktop
    // clears the selection on focus loss. Without this the menu would be built
    // from the selection and then act on an empty one.
    if (m_contextMenu)
        return;

    if (m_selectionModel->hasSelection())
        m_selectionModel->clear();
}

void FolderModel::pinSelection()
{
    m_pinnedSelection = m_selectionModel->selection();
    m_selectionPinned = true;
    m_pinnedSelectionChanged = false;
}

void FolderModel::unpinSelection()
{
    m_pinnedSelection = QItemSelection();
    m_selectionPinned = false;

    if (m_pinnedSelectionChanged) {
        m_pinnedSelectionChanged = false;
        updateSelectedItemsSize();
    }
}

// The cell a new folder or file should land on, in view coordinates plus the
// grid cell under them. Consumed by the next newFolder()/newTextFile();
// (-1, -1) means "wherever it fits".

void FolderModel::setNewDocumentPosition(int x, int y, int cell)
{
    m_newDocumentPos = QPoint(x, y);
    m_newDocumentCell = cell;
}

// Hands the pending cell over to the drop position map, which places the file
// on it as soon as the lister reports it. The cell lets the positioner insert
// the row there in the first place, so the icon never shows up elsewhere first;
// the view coordinates remain the fallback should that cell be taken by then.

void FolderModel::rememberNewDocumentPosition(const QString &name)
{
    if (m_newDocumentPos.x() < 0 || m_newDocumentPos.y() < 0)
        return;

    m_dropTargetPositions.insert(name, m_newDocumentPos);
    if (m_newDocumentCell >= 0)
        m_newDocumentCells.insert(name, m_newDocumentCell);
    m_dropTargetPositionsCleanup->start();
    m_newDocumentPos = QPoint(-1, -1);
    m_newDocumentCell = -1;
}

int FolderModel::newDocumentCell(const QString &name) const
{
    return m_newDocumentCells.value(name, -1);
}

void FolderModel::reserveCell(const QString &name, int cell)
{
    if (cell < 0)
        return;

    m_newDocumentCells.insert(name, cell);
    m_dropTargetPositionsCleanup->start();
}

void FolderModel::setDragHotSpotScrollOffset(int x, int y)
{
    m_dragHotSpotScrollOffset.setX(x);
    m_dragHotSpotScrollOffset.setY(y);
}

void FolderModel::beginItemDragImage(int row)
{
    if (row >= 0)
        m_pendingDragImages.insert(row);
}

void FolderModel::cancelItemDragImage(int row)
{
    if (m_pendingDragImages.remove(row) && m_pendingDragImages.isEmpty())
        startPendingDrag();
}

void FolderModel::addItemDragImage(int row, int x, int y, int width, int height,
                                   const QVariant &image)
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

    if (m_pendingDragImages.remove(row) && m_pendingDragImages.isEmpty())
        startPendingDrag();
}

// Moves an already grabbed snapshot to where the icon sits now. Returns false
// when there is nothing stored for the row, so the caller can grab one.

bool FolderModel::updateItemDragRect(int row, int x, int y, int width, int height)
{
    DragImage *image = m_dragImages.value(row);

    if (!image)
        return false;

    image->rect = QRect(x, y, width, height);

    return true;
}

void FolderModel::clearDragImages()
{
    qDeleteAll(m_dragImages);
    m_dragImages.clear();

    if (!m_pendingDragImages.isEmpty()) {
        m_pendingDragImages.clear();
        startPendingDrag();
    }
}

void FolderModel::dragSelected(int x, int y)
{
    if (m_dragInProgress)
        return;

    m_dragInProgress = true;
    emit draggingChanged();

    m_pendingDragPos = QPoint(x, y);
    m_dragWaitingForImages = true;

    // Pressing an unselected icon selects it and starts its snapshot, which
    // only arrives after a frame; starting the drag first leaves no pixmap.
    if (m_pendingDragImages.isEmpty())
        startPendingDrag();
    else
        m_dragImageTimeout->start();
}

void FolderModel::startPendingDrag()
{
    if (!m_dragWaitingForImages)
        return;

    m_dragWaitingForImages = false;
    m_dragImageTimeout->stop();

    QMetaObject::invokeMethod(this, "dragSelectedInternal", Qt::QueuedConnection,
                              Q_ARG(int, m_pendingDragPos.x()), Q_ARG(int, m_pendingDragPos.y()));
}

QUrl FolderModel::dropTargetUrl(int row) const
{
    Cutefish::IO::FileItem item;

    if (row >= 0) {
        if (row >= rowCount())
            return {};
        item = itemForIndex(index(row, 0));
        if (!item.isDir())
            return {};
    } else {
        item = rootItem();
    }

    QUrl targetUrl = item.isNull() ? m_dirModel->dirLister()->url() : item.mostLocalUrl();
    if (targetUrl.fileName() == QLatin1Char('.'))
        targetUrl = targetUrl.adjusted(QUrl::RemoveFilename);

    return targetUrl;
}

bool FolderModel::canDropUrls(const QList<QUrl> &urls, int row, Qt::DropAction action) const
{
    if (urls.isEmpty() || action == Qt::IgnoreAction)
        return false;

    const QUrl targetUrl = dropTargetUrl(row);
    if (targetUrl.isEmpty())
        return false;

    if (targetUrl.scheme() != QLatin1String("trash")) {
        const QString targetPath = targetUrl.toLocalFile();
        const QFileInfo targetInfo(targetPath);
        if (targetPath.isEmpty() || !targetInfo.isDir() || !targetInfo.isWritable()
            || !targetInfo.isExecutable())
            return false;
    }

    if (m_isDesktop && m_dragInProgress && row == -1
        && isDropBetweenSharedViews(urls, targetUrl))
        return true;

    const QUrl normalizedTarget =
            targetUrl.adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash);
    const QString targetPath = QDir::cleanPath(targetUrl.toLocalFile());
    const bool moving = action == Qt::MoveAction || action == Qt::TargetMoveAction;

    for (const QUrl &url : urls) {
        const QUrl normalizedSource =
                url.adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash);
        if (normalizedSource == normalizedTarget)
            return false;

        if (url.isLocalFile() && targetUrl.isLocalFile()) {
            const QString sourcePath = QDir::cleanPath(url.toLocalFile());
            if (QFileInfo(sourcePath).absolutePath() == targetPath || targetPath == sourcePath
                || targetPath.startsWith(sourcePath + QLatin1Char('/')))
                return false;
            if (moving) {
                const QFileInfo sourceParent(QFileInfo(sourcePath).absolutePath());
                if (!sourceParent.isWritable() || !sourceParent.isExecutable())
                    return false;
            }
        }

        if (moving && !Cutefish::IO::canMoveStandardFolderTo(url, targetUrl))
            return false;
    }
    return true;
}

bool FolderModel::canDrop(QObject *mimeDataObject, int row, int action) const
{
    const auto *mimeData = qobject_cast<QMimeData *>(mimeDataObject);
    return mimeData
            && canDropUrls(mimeData->urls(), row, static_cast<Qt::DropAction>(action));
}

void FolderModel::drop(QQuickItem *target, QObject *dropEvent, int row)
{
    QMimeData *mimeData =
            qobject_cast<QMimeData *>(dropEvent->property("mimeData").value<QObject *>());

    // Consume whatever cell the view set for this drop, whether or not it is used.
    const int cell = m_newDocumentCell;
    m_newDocumentPos = QPoint(-1, -1);
    m_newDocumentCell = -1;

    if (!mimeData) {
        return;
    }

    const QVariant acceptedAction = dropEvent->property("dropAction");
    const auto action = static_cast<Qt::DropAction>(
            acceptedAction.isValid() ? acceptedAction.toInt()
                                     : dropEvent->property("proposedAction").toInt());
    if (!canDropUrls(mimeData->urls(), row, action))
        return;

    const QUrl targetUrl = dropTargetUrl(row);

    QPointF position(dropEvent->property("x").toInt(), dropEvent->property("y").toInt());
    // The desktop drop area covers margins outside the icon grid.
    if (auto *area = qobject_cast<QQuickItem *>(dropEvent->parent()); area && target)
        position = area->mapToItem(target, position);
    const int x = qRound(position.x());
    const int y = qRound(position.y());

    if (m_dragInProgress && row == -1) {
        if (mimeData->urls().isEmpty())
            return;

        setSortMode(-1);

        // The files are already here -- no rows will be inserted for them, so
        // recording drop positions would only leave entries behind that move
        // some later file of the same name to a stale place.
        emit move(x, y, mimeData->urls());

        return;
    }

    if (!isDropBetweenSharedViews(mimeData->urls(), targetUrl)) {
        if (m_isDesktop && row == -1) {
            for (const QUrl &url : mimeData->urls())
                m_dropTargetPositions.insert(url.fileName(), QPoint(x, y));
            // The first file takes the cell itself; the rest spread out from
            // it once they arrive, the way they always have.
            reserveCell(mimeData->urls().first().fileName(), cell);
            m_dropTargetPositionsCleanup->start();
        }
        FileOperationsClient::instance()->drop(mimeData, targetUrl, action);

        // Add select
        for (QUrl url : mimeData->urls()) {
            m_needSelectUrls.append(QUrl::fromLocalFile(
                    QString("%1/%2").arg(rootItem().url().toLocalFile()).arg(url.fileName())));
        }
    }
}

void FolderModel::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    for (const auto &range : selected)
        emit dataChanged(range.topLeft(), range.bottomRight(), {SelectedRole});

    for (const auto &range : deselected)
        emit dataChanged(range.topLeft(), range.bottomRight(), {SelectedRole});

    if (!m_selectionModel->hasSelection()) {
        clearDragImages();
    } else {
        foreach (const QModelIndex &idx, deselected.indexes()) {
            delete m_dragImages.take(idx.row());
            cancelItemDragImage(idx.row());
        }
    }

    updateActions();

    emit selectionCountChanged();

    // A rubber band changes the selection on every pointer move; sizing it
    // waits until the band closes.
    if (m_selectionPinned) {
        m_pinnedSelectionChanged = true;
        return;
    }

    updateSelectedItemsSize();
}

void FolderModel::updateSelectedItemsSize()
{
    // The desktop does not need to calculate the selected file size.
    if (m_isDesktop)
        return;

    // Start calculating file size.
    if (m_sizeJob == nullptr) {
        m_sizeJob = new CFileSizeJob(this);

        connect(m_sizeJob, &CFileSizeJob::sizeChanged, this, [this] {
            m_selectedItemSize = Cutefish::IO::convertSize(m_sizeJob->totalSize());
            if (!m_selectionModel->hasSelection())
                m_selectedItemSize = "";
            emit selectedItemSizeChanged();
        });

        connect(m_sizeJob, &CFileSizeJob::result, this, [this] {
            m_selectedItemSize = Cutefish::IO::convertSize(m_sizeJob->totalSize());
            if (!m_selectionModel->hasSelection())
                m_selectedItemSize = "";
            emit selectedItemSizeChanged();
        });
    }

    m_sizeJob->stop();

    if (!m_selectionModel->hasSelection()) {
        m_sizeJob->blockSignals(true);
        m_selectedItemSize = "";
        emit selectedItemSizeChanged();
    } else {
        bool fileExists = false;

        for (const QModelIndex &index : m_selectionModel->selectedIndexes()) {
            if (itemForIndex(index).isFile()) {
                fileExists = true;
                break;
            }
        }

        // Reion: The size label at the bottom needs to be updated
        // only if you select the include file.
        if (fileExists) {
            m_sizeJob->blockSignals(false);
            m_sizeJob->start(selectedUrls());
        }
    }
}

void FolderModel::dragSelectedInternal(int x, int y)
{
    // The button may have been released while the snapshots were pending.
    if (!m_viewAdapter || !m_selectionModel->hasSelection()
        || !(QGuiApplication::mouseButtons() & Qt::LeftButton)) {
        m_dragInProgress = false;
        emit draggingChanged();
        return;
    }

    QQuickItem *item = qobject_cast<QQuickItem *>(m_viewAdapter->adapterView());
    QDrag *drag = new QDrag(item);
    // Qt deletes the drag once exec() returns; as the view's child it could be
    // deleted inside exec() instead, when the tab closes mid-drag.
    drag->setParent(nullptr);
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

    // exec() runs a nested event loop: the tab (and with it this model and the
    // view) can be closed before it returns.
    QPointer<FolderModel> self(this);
    QPointer<QQuickItem> view(item);

    item->grabMouse();
    drag->exec(supportedDragActions(), Qt::MoveAction);
    if (view)
        view->ungrabMouse();
    if (!self)
        return;

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

void FolderModel::onRowsInserted(const QModelIndex &parent, int first, int last)
{
    if (m_updateNeedSelectTimer->isActive()) {
        m_updateNeedSelectTimer->stop();
    }

    QPersistentModelIndex changeIdx;

    for (int i = first; i <= last; ++i) {
        const auto idx = index(i, 0, parent);
        const auto url = itemForIndex(idx).url();
        m_newDocumentCells.remove(url.fileName());
        auto it = m_dropTargetPositions.find(url.fileName());
        if (it != m_dropTargetPositions.end()) {
            const auto pos = it.value();
            m_dropTargetPositions.erase(it);
            // Queued: the views are still inside the insertion transaction, and
            // repositioning the row from within it would reenter their models.
            QMetaObject::invokeMethod(
                    this,
                    [this, pos, url] {
                        Q_EMIT move(pos.x(), pos.y(), {url});
                    },
                    Qt::QueuedConnection);
        }

        if (url == m_newDocumentUrl) {
            changeIdx = idx;
            m_newDocumentUrl.clear();
        }
    }

    // A new folder is selected before the rename is requested.
    QTimer::singleShot(m_updateNeedSelectTimer->interval() + 10, this, [this, changeIdx] {
        if (changeIdx.isValid()) {
            setSelected(changeIdx.row());
            emit requestRename();
        }
    });

    m_updateNeedSelectTimer->start();
}

void FolderModel::delayUpdateNeedSelectUrls()
{
    QTimer::singleShot(100, this, &FolderModel::updateNeedSelectUrls);
}

void FolderModel::updateNeedSelectUrls()
{
    QModelIndexList needSelectList;

    for (const QUrl &url : m_needSelectUrls) {
        const QModelIndex &idx = indexForUrl(url);

        if (!idx.isValid())
            continue;

        const QModelIndex &sourceIdx = idx;

        needSelectList.append(sourceIdx);
    }

    m_needSelectUrls.clear();

    // If the selected item already exists, clear it immediately.
    if (!needSelectList.isEmpty()) {
        clearSelection();
    }

    for (const QModelIndex &idx : needSelectList) {
        setSelected(idx.row());
    }
}

void FolderModel::selectUrlsWhenListed(const QStringList &urls)
{
    m_pendingSelection.clear();
    for (const QString &url : urls)
        m_pendingSelection.append(resolve(url));

    if (m_status == Ready)
        applyPendingSelection();
}

void FolderModel::applyPendingSelection()
{
    if (m_pendingSelection.isEmpty())
        return;

    const QList<QUrl> urls = std::exchange(m_pendingSelection, {});
    int first = -1;
    for (const QUrl &url : urls) {
        const QModelIndex index = indexForUrl(url);
        if (!index.isValid())
            continue;
        if (first < 0) {
            clearSelection();
            first = index.row();
        }
        setSelected(index.row());
    }

    if (first >= 0)
        emit scrollToItem(first);
}

void FolderModel::addDragImage(QDrag *drag, int x, int y)
{
    if (!drag || m_dragImages.isEmpty())
        return;

    QRegion region;

    // A row that is not selected any more has a stale image and stale geometry;
    // compositing it would drag the wrong picture, or place it far off-cursor.
    for (auto it = m_dragImages.begin(); it != m_dragImages.end();) {
        if (!isSelected(it.key())) {
            delete it.value();
            it = m_dragImages.erase(it);
        } else {
            ++it;
        }
    }

    if (m_dragImages.isEmpty())
        return;

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
