/***************************************************************************
 *   Copyright (C) 2014 by Eike Hein <hein@kde.org>                   *
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

#include "positioner.h"
#include "foldermodel.h"

#include <QDebug>
#include <QTimer>

#include <cstdlib>
#include <utility>

Positioner::Positioner(QObject *parent)
    : QAbstractItemModel(parent)
    , m_enabled(false)
    , m_folderModel(nullptr)
    , m_perStripe(0)
    , m_stripes(0)
    , m_ignoreNextTransaction(false)
    , m_deferApplyPositions(false)
    , m_updatePositionsTimer(new QTimer(this))
{
    m_updatePositionsTimer->setSingleShot(true);
    m_updatePositionsTimer->setInterval(0);
    connect(m_updatePositionsTimer, &QTimer::timeout, this, &Positioner::updatePositions);
}

Positioner::~Positioner()
{
}

bool Positioner::enabled() const
{
    return m_enabled;
}

void Positioner::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;

        beginResetModel();

        if (enabled && m_folderModel) {
            initMaps();
        }

        endResetModel();

        emit enabledChanged();

        if (!enabled) {
            m_updatePositionsTimer->start();
        }
    }
}

FolderModel *Positioner::folderModel() const
{
    return m_folderModel;
}

void Positioner::setFolderModel(QObject *folderModel)
{
    if (m_folderModel != folderModel) {
        beginResetModel();

        if (m_folderModel) {
            disconnectSignals(m_folderModel);
        }

        m_folderModel = qobject_cast<FolderModel *>(folderModel);

        if (m_folderModel) {
            connectSignals(m_folderModel);

            if (m_enabled) {
                initMaps();
            }
        }

        endResetModel();

        emit folderModelChanged();
    }
}

int Positioner::perStripe() const
{
    return m_perStripe;
}

void Positioner::setPerStripe(int perStripe)
{
    if (m_perStripe != perStripe) {
        m_perStripe = perStripe;

        emit perStripeChanged();

        if (m_enabled && perStripe > 0 && !m_proxyToSource.isEmpty()) {
            applyPositions();
        }
    }
}

int Positioner::stripes() const
{
    return m_stripes;
}

void Positioner::setStripes(int stripes)
{
    if (m_stripes != stripes) {
        m_stripes = stripes;

        emit stripesChanged();

        if (m_enabled && m_stripes > 0 && m_perStripe > 0 && !m_proxyToSource.isEmpty()) {
            applyPositions();
        }
    }
}

// A cell is usable only while it is inside the grid the view currently has
// room for; anything outside has to be reflowed or it would sit off-screen.
bool Positioner::fitsGrid(int stripe, int pos) const
{
    if (stripe < 0 || pos < 0)
        return false;

    if (m_perStripe > 0 && pos >= m_perStripe)
        return false;

    if (m_stripes > 0 && stripe >= m_stripes)
        return false;

    return true;
}

QStringList Positioner::positions() const
{
    return m_positions;
}

void Positioner::setPositions(const QStringList &positions)
{
    if (m_positions != positions) {
        m_positions = positions;

        emit positionsChanged();

        // Defer applying positions until listing completes.
        if (m_folderModel->status() == FolderModel::Listing) {
            m_deferApplyPositions = true;
        } else {
            applyPositions();
        }
    }
}

int Positioner::map(int row) const
{
    if (m_enabled && m_folderModel) {
        return m_proxyToSource.value(row, -1);
    }

    return row;
}

int Positioner::mapFromSource(int row) const
{
    if (m_enabled && m_folderModel) {
        return m_sourceToProxy.value(row, -1);
    }

    return row;
}

int Positioner::nearestItem(int currentIndex, Qt::ArrowType direction)
{
    const int count = rowCount();

    if (count <= 0 || currentIndex >= count) {
        return -1;
    }

    // Without stored positions the grid is always packed, so navigation is
    // plain index arithmetic along and across the stripes.
    if (!m_enabled) {
        if (m_perStripe <= 0) {
            return -1;
        }

        if (currentIndex < 0) {
            return 0;
        }

        int next = currentIndex;

        switch (direction) {
        case Qt::LeftArrow:
            next -= 1;
            break;
        case Qt::RightArrow:
            next += 1;
            break;
        case Qt::UpArrow:
            next -= m_perStripe;
            break;
        case Qt::DownArrow:
            next += m_perStripe;
            break;
        default:
            return -1;
        }

        return (next >= 0 && next < count) ? next : -1;
    }

    if (currentIndex < 0) {
        return firstRow();
    }

    int hDirection = 0;
    int vDirection = 0;

    switch (direction) {
    case Qt::LeftArrow:
        hDirection = -1;
        break;
    case Qt::RightArrow:
        hDirection = 1;
        break;
    case Qt::UpArrow:
        vDirection = -1;
        break;
    case Qt::DownArrow:
        vDirection = 1;
        break;
    default:
        return -1;
    }

    QList<int> rows(m_proxyToSource.keys());
    std::sort(rows.begin(), rows.end());

    int nearestItem = -1;
    const QPoint currentPos(currentIndex % m_perStripe, currentIndex / m_perStripe);
    int lastDistance = -1;
    int distance = 0;

    foreach (int row, rows) {
        const QPoint pos(row % m_perStripe, row / m_perStripe);

        if (row == currentIndex) {
            continue;
        }

        if (hDirection == 0) {
            if (vDirection * pos.y() > vDirection * currentPos.y()) {
                distance = (pos - currentPos).manhattanLength();

                if (nearestItem == -1 || distance < lastDistance || (distance == lastDistance && pos.x() == currentPos.x())) {
                    nearestItem = row;
                    lastDistance = distance;
                }
            }
        } else if (vDirection == 0) {
            if (hDirection * pos.x() > hDirection * currentPos.x()) {
                distance = (pos - currentPos).manhattanLength();

                if (nearestItem == -1 || distance < lastDistance || (distance == lastDistance && pos.y() == currentPos.y())) {
                    nearestItem = row;
                    lastDistance = distance;
                }
            }
        }
    }

    return nearestItem;
}

bool Positioner::isBlank(int row) const
{
    if (!m_enabled && m_folderModel) {
        return m_folderModel->isBlank(row);
    }

    if (m_proxyToSource.contains(row) && m_folderModel && !m_folderModel->isBlank(m_proxyToSource.value(row))) {
        return false;
    }

    return true;
}

int Positioner::indexForUrl(const QUrl &url) const
{
    if (!m_folderModel) {
        return -1;
    }

    const QString &name = url.fileName();

    int sourceIndex = -1;

    // TODO Optimize.
    for (int i = 0; i < m_folderModel->rowCount(); ++i) {
        if (m_folderModel->data(m_folderModel->index(i, 0), FolderModel::FileNameRole).toString() == name) {
            sourceIndex = i;

            break;
        }
    }

    return m_sourceToProxy.value(sourceIndex, -1);
}

void Positioner::setRangeSelected(int anchor, int to)
{
    if (!m_folderModel) {
        return;
    }

    if (m_enabled) {
        QVariantList indices;

        for (int i = qMin(anchor, to); i <= qMax(anchor, to); ++i) {
            if (m_proxyToSource.contains(i)) {
                indices.append(m_proxyToSource.value(i));
            }
        }

        if (!indices.isEmpty()) {
            m_folderModel->updateSelection(indices, false);
        }
    } else {
        m_folderModel->setRangeSelected(anchor, to);
    }
}

QHash<int, QByteArray> Positioner::roleNames() const
{
    return FolderModel::staticRoleNames();
}

QModelIndex Positioner::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex Positioner::parent(const QModelIndex &index) const
{
    if (m_folderModel) {
        m_folderModel->parent(index);
    }

    return QModelIndex();
}

QVariant Positioner::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (m_folderModel) {
        if (m_enabled) {
            if (m_proxyToSource.contains(index.row())) {
                return m_folderModel->data(m_folderModel->index(m_proxyToSource.value(index.row()), 0), role);
            }

            // An empty cell still has to answer with the type the delegate
            // expects, or every binding on it warns about an undefined value.
            switch (role) {
            case FolderModel::BlankRole:
                return true;
            case FolderModel::SelectedRole:
            case FolderModel::IsDirRole:
            case FolderModel::IsHiddenRole:
            case FolderModel::IsLinkRole:
            case FolderModel::IsDesktopFileRole:
                return false;
            case FolderModel::UrlRole:
            case FolderModel::DisplayNameRole:
            case FolderModel::FileNameRole:
            case FolderModel::FileSizeRole:
            case FolderModel::IconNameRole:
            case FolderModel::ThumbnailRole:
            case FolderModel::ModifiedRole:
                return QString();
            default:
                break;
            }
        } else {
            return m_folderModel->data(m_folderModel->index(index.row(), 0), role);
        }
    }

    return QVariant();
}

int Positioner::rowCount(const QModelIndex &parent) const
{
    if (m_folderModel) {
        if (m_enabled) {
            if (parent.isValid()) {
                return 0;
            } else {
                return lastRow() + 1;
            }
        } else {
            return m_folderModel->rowCount(parent);
        }
    }

    return 0;
}

int Positioner::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    if (m_folderModel) {
        return 1;
    }

    return 0;
}

void Positioner::reset()
{
    if (m_positions.isEmpty())
        return;

    beginResetModel();

    initMaps();

    endResetModel();

    m_positions = QStringList();
    emit positionsChanged();
}

void Positioner::move(const QVariantList &moves)
{
    // Don't allow moves while listing.
    if (m_folderModel->status() == FolderModel::Listing) {
        m_deferMovePositions = moves;
        return;
    }

    QVector<int> fromIndices;
    QVector<int> toIndices;
    QVector<int> sourceRows;

    for (int i = 0; i < moves.count(); ++i) {
        const int isFrom = (i % 2 == 0);
        const int v = moves[i].toInt();

        if (isFrom) {
            if (m_proxyToSource.contains(v)) {
                sourceRows.append(m_proxyToSource.value(v));
            } else {
                sourceRows.append(-1);
            }
        }

        (isFrom ? fromIndices : toIndices).append(v);
    }

    const int oldCount = rowCount();

    for (int i = 0; i < fromIndices.count(); ++i) {
        const int from = fromIndices[i];
        int to = toIndices[i];
        const int sourceRow = sourceRows[i];

        if (sourceRow == -1 || from == to) {
            continue;
        }

        if (to == -1) {
            to = firstFreeRow();

            if (to == -1) {
                to = lastRow() + 1;
            }
        }

        if (!fromIndices.contains(to) && !isBlank(to)) {
            /* find the next blank space
             * we won't be happy if we're moving two icons to the same place
             */
            const int cells = (m_perStripe > 0 && m_stripes > 0) ? (m_perStripe * m_stripes) : 0;
            int tried = 0;

            while ((!isBlank(to) && from != to) || toIndices.contains(to)) {
                to++;

                // Wrap inside the grid: an occupied cell at the far corner must
                // not push the icon into a column the desktop cannot show.
                if (cells > 0 && to >= cells) {
                    to = 0;
                }

                if (cells > 0 && ++tried > cells) {
                    // Every cell is taken; park it after the last one.
                    to = lastRow() + 1;
                    break;
                }
            }
        }

        toIndices[i] = to;

        if (!toIndices.contains(from)) {
            m_proxyToSource.remove(from);
        }

        updateMaps(to, sourceRow);

        const QModelIndex &fromIdx = index(from, 0);
        emit dataChanged(fromIdx, fromIdx);

        if (to < oldCount) {
            const QModelIndex &toIdx = index(to, 0);
            emit dataChanged(toIdx, toIdx);
        }
    }

    const int newCount = rowCount();

    if (newCount > oldCount) {
        if (m_beginInsertRowsCalled) {
            endInsertRows();
            m_beginInsertRowsCalled = false;
        }
        beginInsertRows(QModelIndex(), oldCount, newCount - 1);
        endInsertRows();
    }

    if (newCount < oldCount) {
        beginRemoveRows(QModelIndex(), newCount, oldCount - 1);
        endRemoveRows();
    }

    m_updatePositionsTimer->start();
}

void Positioner::updatePositions()
{
    QStringList positions;

    if (m_enabled && !m_proxyToSource.isEmpty() && m_perStripe > 0) {
        positions.append(QString::number((1 + ((rowCount() - 1) / m_perStripe))));
        positions.append(QString::number(m_perStripe));

        // Sorted by cell so the stored layout is stable: an unchanged desktop
        // must not produce a different string list every time.
        QList<int> rows(m_proxyToSource.keys());
        std::sort(rows.begin(), rows.end());

        for (int row : std::as_const(rows)) {
            const QString &name = m_folderModel->data(m_folderModel->index(m_proxyToSource.value(row), 0), FolderModel::UrlRole).toString();

            if (name.isEmpty()) {
                qDebug() << this << m_proxyToSource.value(row) << "Source model doesn't know this index!";

                return;
            }

            positions.append(name);
            positions.append(QString::number(qMax(0, row / m_perStripe)));
            positions.append(QString::number(qMax(0, row % m_perStripe)));
        }
    }

    if (positions != m_positions) {
        m_positions = positions;

        emit positionsChanged();
    }
}

void Positioner::sourceStatusChanged()
{
    if (m_folderModel->status() != FolderModel::Listing && (m_deferApplyPositions || m_positions.size() >= 5)) {
        applyPositions();
    }

    if (m_deferMovePositions.count() && m_folderModel->status() != FolderModel::Listing) {
        move(m_deferMovePositions);
        m_deferMovePositions.clear();
    }
}

void Positioner::sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    if (m_enabled) {
        // A rename keeps the icon where it is but changes the url the stored
        // layout knows it by, so the records have to be rewritten.
        if (roles.isEmpty() || roles.contains(FolderModel::UrlRole)) {
            m_updatePositionsTimer->start();
        }

        int start = topLeft.row();
        int end = bottomRight.row();

        for (int i = start; i <= end; ++i) {
            if (m_sourceToProxy.contains(i)) {
                const QModelIndex &idx = index(m_sourceToProxy.value(i), 0);

                emit dataChanged(idx, idx);
            }
        }
    } else {
        emit dataChanged(topLeft, bottomRight, roles);
    }
}

void Positioner::sourceModelAboutToBeReset()
{
    beginResetModel();
}

void Positioner::sourceModelReset()
{
    if (m_enabled) {
        QHash<int, int> proxyToSource;
        QHash<int, int> sourceToProxy;

        if (computeMaps(&proxyToSource, &sourceToProxy)) {
            m_proxyToSource = proxyToSource;
            m_sourceToProxy = sourceToProxy;
        } else {
            initMaps();
        }
    }

    endResetModel();
}

void Positioner::sourceRowsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    if (m_enabled) {
        // Don't insert yet if we're waiting for listing to complete to apply
        // initial positions;
        if (m_deferApplyPositions) {
            return;
        } else if (m_proxyToSource.isEmpty()) {
            // First icons of an empty desktop. They go on their default cells,
            // and the untaken cells before them are inserted as blanks so the
            // announced range matches the row count that follows.
            QHash<int, int> cells;
            int lastNew = -1;

            for (int i = start; i <= end; ++i) {
                cells.insert(i, defaultCell(i - start));
                lastNew = qMax(lastNew, cells.value(i));
            }

            beginInsertRows(parent, 0, lastNew);
            m_beginInsertRowsCalled = true;

            for (int i = start; i <= end; ++i) {
                updateMaps(cells.value(i), i);
            }

            return;
        }

        // When new rows are inserted, they might go in the beginning or in the middle.
        // In this case we must update first the existing proxy->source and source->proxy
        // mapping, otherwise the proxy items will point to the wrong source item.
        int count = end - start + 1;
        m_sourceToProxy.clear();
        for (auto it = m_proxyToSource.begin(); it != m_proxyToSource.end(); ++it) {
            int sourceIdx = *it;
            if (sourceIdx >= start) {
                *it += count;
            }
            m_sourceToProxy[*it] = it.key();
        }

        int free = -1;
        int rest = -1;

        for (int i = start; i <= end; ++i) {
            free = firstFreeRow();

            if (free != -1) {
                updateMaps(free, i);
                m_pendingChanges << createIndex(free, 0);
            } else {
                rest = i;
                break;
            }
        }

        if (rest != -1) {
            const int firstNew = lastRow() + 1;
            const int remainder = (end - rest);

            // Nothing is free inside the existing rows, so the new icons take
            // default cells beyond them; every cell up to the last one used is
            // part of the insertion, blank or not.
            QHash<int, int> occupied = m_proxyToSource;
            QList<int> cells;
            int lastNew = firstNew + remainder;

            for (int i = 0; i <= remainder; ++i) {
                int cell = nextDefaultCell(occupied);

                if (cell < firstNew) {
                    cell = firstNew + i;
                }

                occupied.insert(cell, rest + i);
                cells.append(cell);
                lastNew = qMax(lastNew, cell);
            }

            beginInsertRows(parent, firstNew, lastNew);
            m_beginInsertRowsCalled = true;

            for (int i = 0; i <= remainder; ++i) {
                updateMaps(cells.at(i), rest + i);
            }
        } else {
            m_ignoreNextTransaction = true;
        }
    } else {
        beginInsertRows(parent, start, end);
        m_beginInsertRowsCalled = true;
    }
}

void Positioner::sourceRowsAboutToBeMoved(const QModelIndex &sourceParent,
                                          int sourceStart,
                                          int sourceEnd,
                                          const QModelIndex &destinationParent,
                                          int destinationRow)
{
    emit beginMoveRows(sourceParent, sourceStart, sourceEnd, destinationParent, destinationRow);
}

void Positioner::sourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    if (m_enabled) {
        int oldLast = lastRow();

        for (int i = first; i <= last; ++i) {
            int proxyRow = m_sourceToProxy.take(i);
            m_proxyToSource.remove(proxyRow);
            m_pendingChanges << createIndex(proxyRow, 0);
        }

        QHash<int, int> newProxyToSource;
        QHash<int, int> newSourceToProxy;
        QHashIterator<int, int> it(m_sourceToProxy);
        int delta = std::abs(first - last) + 1;

        while (it.hasNext()) {
            it.next();

            if (it.key() > last) {
                newProxyToSource.insert(it.value(), it.key() - delta);
                newSourceToProxy.insert(it.key() - delta, it.value());
            } else {
                newProxyToSource.insert(it.value(), it.key());
                newSourceToProxy.insert(it.key(), it.value());
            }
        }

        m_proxyToSource = newProxyToSource;
        m_sourceToProxy = newSourceToProxy;

        int newLast = lastRow();

        if (oldLast > newLast) {
            int diff = oldLast - newLast;
            beginRemoveRows(QModelIndex(), ((oldLast - diff) + 1), oldLast);
        } else {
            m_ignoreNextTransaction = true;
        }
    } else {
        beginRemoveRows(parent, first, last);
    }
}

void Positioner::sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint)
{
    Q_UNUSED(parents)

    emit layoutAboutToBeChanged(QList<QPersistentModelIndex>(), hint);
}

void Positioner::sourceRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    if (!m_ignoreNextTransaction) {
        if (m_beginInsertRowsCalled) {
            endInsertRows();
            m_beginInsertRowsCalled = false;
        }
    } else {
        m_ignoreNextTransaction = false;
    }

    flushPendingChanges();

    // Don't generate new positions data if we're waiting for listing to
    // complete to apply initial positions.
    if (!m_deferApplyPositions) {
        m_updatePositionsTimer->start();
    }
}

void Positioner::sourceRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow)
{
    Q_UNUSED(sourceParent)
    Q_UNUSED(sourceStart)
    Q_UNUSED(sourceEnd)
    Q_UNUSED(destinationParent)
    Q_UNUSED(destinationRow)

    emit endMoveRows();
}

void Positioner::sourceRowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    if (!m_ignoreNextTransaction) {
        emit endRemoveRows();
    } else {
        m_ignoreNextTransaction = false;
    }

    flushPendingChanges();

    m_updatePositionsTimer->start();
}

void Positioner::sourceLayoutChanged(const QList<QPersistentModelIndex> &parents, QAbstractItemModel::LayoutChangeHint hint)
{
    Q_UNUSED(parents)

    // A re-sort or re-filter renumbers the source rows; the icons stay on their
    // cells, so remap by url instead of repacking the whole desktop.
    if (m_enabled) {
        QHash<int, int> proxyToSource;
        QHash<int, int> sourceToProxy;

        if (computeMaps(&proxyToSource, &sourceToProxy)) {
            m_proxyToSource = proxyToSource;
            m_sourceToProxy = sourceToProxy;
        } else {
            initMaps();
        }
    }

    emit layoutChanged(QList<QPersistentModelIndex>(), hint);
}

void Positioner::initMaps()
{
    m_proxyToSource.clear();
    m_sourceToProxy.clear();

    const int size = m_folderModel->rowCount();

    for (int i = 0; i < size; ++i) {
        updateMaps(defaultCell(i), i);
    }
}

// Cell of the nth default slot: the rightmost stripe first, filled from its
// start, then the stripe next to it -- where macOS puts new desktop icons.
// Falls back to plain packing while the grid size is still unknown.
int Positioner::defaultCell(int ordinal) const
{
    if (m_perStripe <= 0 || m_stripes <= 0) {
        return ordinal;
    }

    // More icons than the grid has room for: the extra ones spill past it,
    // where they were piling up before there was a default order.
    if (ordinal >= m_perStripe * m_stripes) {
        return ordinal;
    }

    const int stripe = m_stripes - 1 - (ordinal / m_perStripe);

    return (stripe * m_perStripe) + (ordinal % m_perStripe);
}

// The first default slot no icon sits on.
int Positioner::nextDefaultCell(const QHash<int, int> &occupied) const
{
    if (m_perStripe <= 0 || m_stripes <= 0) {
        return -1;
    }

    const int cellCount = m_perStripe * m_stripes;

    for (int ordinal = 0; ordinal < cellCount; ++ordinal) {
        const int cell = defaultCell(ordinal);

        if (!occupied.contains(cell)) {
            return cell;
        }
    }

    return -1;
}

void Positioner::updateMaps(int proxyIndex, int sourceIndex)
{
    m_proxyToSource.insert(proxyIndex, sourceIndex);
    m_sourceToProxy.insert(sourceIndex, proxyIndex);
}

int Positioner::firstRow() const
{
    if (!m_proxyToSource.isEmpty()) {
        QList<int> keys(m_proxyToSource.keys());
        std::sort(keys.begin(), keys.end());

        return keys.first();
    }

    return -1;
}

// -1 when nothing is mapped, so that lastRow() + 1 is the first free cell and
// the row count of an empty grid, rather than one phantom cell in both.
int Positioner::lastRow() const
{
    if (!m_proxyToSource.isEmpty()) {
        QList<int> keys(m_proxyToSource.keys());
        std::sort(keys.begin(), keys.end());
        return keys.last();
    }

    return -1;
}

int Positioner::firstFreeRow() const
{
    if (m_proxyToSource.isEmpty()) {
        return -1;
    }

    const int last = lastRow();
    const int cell = nextDefaultCell(m_proxyToSource);

    // Past the rows the model already has: the caller has to announce an
    // insertion for it, so leave it to the append path.
    if (cell != -1) {
        return cell <= last ? cell : -1;
    }

    // No grid to go by: fall back to the first gap.
    if (m_perStripe <= 0 || m_stripes <= 0) {
        for (int i = 0; i <= last; ++i) {
            if (!m_proxyToSource.contains(i)) {
                return i;
            }
        }
    }

    return -1;
}

void Positioner::applyPositions()
{
    // We were called while the source model is listing. Defer applying positions
    // until listing completes.
    if (m_folderModel->status() == FolderModel::Listing) {
        m_deferApplyPositions = true;

        return;
    }

    QHash<int, int> proxyToSource;
    QHash<int, int> sourceToProxy;

    if (!computeMaps(&proxyToSource, &sourceToProxy)) {
        // We were waiting for listing to complete before proxying source rows,
        // but we don't have positions to apply. Reset to populate.
        if (m_deferApplyPositions) {
            m_deferApplyPositions = false;
            reset();
        }

        return;
    }

    // Resetting the model tears down every delegate -- and with them the rename
    // editor -- so only do it when the cells actually move.
    if (proxyToSource != m_proxyToSource) {
        beginResetModel();

        m_proxyToSource = proxyToSource;
        m_sourceToProxy = sourceToProxy;

        endResetModel();
    }

    m_deferApplyPositions = false;

    m_updatePositionsTimer->start();
}

// Works out which source row belongs in which cell from the stored positions,
// without touching the live maps: the caller decides how to announce the change.
bool Positioner::computeMaps(QHash<int, int> *proxyToSource, QHash<int, int> *sourceToProxy) const
{
    if (m_positions.size() < 5) {
        return false;
    }

    const QStringList positions = m_positions.mid(2);

    if (positions.count() % 3 != 0) {
        return false;
    }

    proxyToSource->clear();
    sourceToProxy->clear();

    auto lastCell = [](const QHash<int, int> &map) {
        int last = -1;
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            last = qMax(last, it.key());
        }
        return last;
    };

    auto freeCell = [&lastCell, this](const QHash<int, int> &map) {
        const int cell = nextDefaultCell(map);

        if (cell != -1) {
            return cell;
        }

        const int last = lastCell(map);
        for (int i = 0; i <= last; ++i) {
            if (!map.contains(i)) {
                return i;
            }
        }
        return last + 1;
    };

    // The free cell closest to (stripe, pos), clamped into the grid: an icon that
    // no longer fits stays near where it was instead of piling up in the corner.
    auto nearestFree = [&](int stripe, int pos, const QHash<int, int> &map) {
        if (m_perStripe <= 0 || m_stripes <= 0) {
            return -1;
        }

        stripe = qBound(0, stripe, m_stripes - 1);
        pos = qBound(0, pos, m_perStripe - 1);

        for (int radius = 0; radius < m_stripes + m_perStripe; ++radius) {
            int best = -1;
            int bestDistance = 0;

            for (int ds = -radius; ds <= radius; ++ds) {
                for (int dp = -radius; dp <= radius; ++dp) {
                    if (qMax(qAbs(ds), qAbs(dp)) != radius) {
                        continue;
                    }

                    const int s = stripe + ds;
                    const int p = pos + dp;

                    if (s < 0 || p < 0 || s >= m_stripes || p >= m_perStripe) {
                        continue;
                    }

                    const int cell = (s * m_perStripe) + p;

                    if (map.contains(cell)) {
                        continue;
                    }

                    const int distance = (ds * ds) + (dp * dp);

                    if (best == -1 || distance < bestDistance) {
                        best = cell;
                        bestDistance = distance;
                    }
                }
            }

            if (best != -1) {
                return best;
            }
        }

        return -1;
    };

    auto place = [&](int cell, int sourceRow) {
        proxyToSource->insert(cell, sourceRow);
        sourceToProxy->insert(sourceRow, cell);
    };

    QHash<QString, int> sourceIndices;

    for (int i = 0; i < m_folderModel->rowCount(); ++i) {
        sourceIndices.insert(m_folderModel->data(m_folderModel->index(i, 0), FolderModel::UrlRole).toString(), i);
    }

    // Records that no longer fit the current grid, with the cell they asked for.
    struct Spill {
        QString name;
        int stripe;
        int pos;
    };
    QVector<Spill> spilled;

    for (int i = 0; i < positions.count() / 3; ++i) {
        const int offset = i * 3;
        const QString name = positions.at(offset);

        bool ok = false;
        const int stripe = positions.at(offset + 1).toInt(&ok);
        if (!ok) {
            continue;
        }

        const int pos = positions.at(offset + 2).toInt(&ok);
        if (!ok) {
            continue;
        }

        if (!sourceIndices.contains(name)) {
            continue;
        }

        if (!fitsGrid(stripe, pos)) {
            spilled.append({name, stripe, pos});
            continue;
        }

        const int cell = (stripe * m_perStripe) + pos;

        if (proxyToSource->contains(cell)) {
            spilled.append({name, stripe, pos});
            continue;
        }

        place(cell, sourceIndices.take(name));
    }

    // Find new cells for the items that didn't fit, then for source items we
    // have no record of at all.
    for (const Spill &spill : std::as_const(spilled)) {
        if (!sourceIndices.contains(spill.name)) {
            continue;
        }

        int cell = nearestFree(spill.stripe, spill.pos, *proxyToSource);

        if (cell < 0) {
            cell = freeCell(*proxyToSource);
        }

        place(cell, sourceIndices.take(spill.name));
    }

    QStringList newNames = sourceIndices.keys();
    std::sort(newNames.begin(), newNames.end());

    for (const QString &name : std::as_const(newNames)) {
        const int sourceRow = sourceIndices.value(name);

        // A renamed file has no record under its new url, but it is already on
        // a cell and must stay there; only genuinely new items get a free cell.
        int cell = m_sourceToProxy.value(sourceRow, -1);

        if (cell < 0 || proxyToSource->contains(cell) || (m_perStripe > 0 && !fitsGrid(cell / m_perStripe, cell % m_perStripe))) {
            cell = freeCell(*proxyToSource);
        }

        place(cell, sourceRow);
    }

    return true;
}

void Positioner::flushPendingChanges()
{
    if (m_pendingChanges.isEmpty()) {
        return;
    }

    int last = lastRow();

    foreach (const QModelIndex &idx, m_pendingChanges) {
        if (idx.row() <= last) {
            emit dataChanged(idx, idx);
        }
    }

    m_pendingChanges.clear();
}

void Positioner::connectSignals(FolderModel *model)
{
    connect(model, &QAbstractItemModel::dataChanged, this, &Positioner::sourceDataChanged, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &Positioner::sourceModelAboutToBeReset, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::modelReset, this, &Positioner::sourceModelReset, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &Positioner::sourceRowsAboutToBeInserted, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this, &Positioner::sourceRowsAboutToBeMoved, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &Positioner::sourceRowsAboutToBeRemoved, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &Positioner::sourceLayoutAboutToBeChanged, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::rowsInserted, this, &Positioner::sourceRowsInserted, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::rowsMoved, this, &Positioner::sourceRowsMoved, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &Positioner::sourceRowsRemoved, Qt::UniqueConnection);
    connect(model, &QAbstractItemModel::layoutChanged, this, &Positioner::sourceLayoutChanged, Qt::UniqueConnection);
    connect(m_folderModel, &FolderModel::urlChanged, this, &Positioner::reset, Qt::UniqueConnection);
    connect(m_folderModel, &FolderModel::statusChanged, this, &Positioner::sourceStatusChanged, Qt::UniqueConnection);
}

void Positioner::disconnectSignals(FolderModel *model)
{
    disconnect(model, &QAbstractItemModel::dataChanged, this, &Positioner::sourceDataChanged);
    disconnect(model, &QAbstractItemModel::modelAboutToBeReset, this, &Positioner::sourceModelAboutToBeReset);
    disconnect(model, &QAbstractItemModel::modelReset, this, &Positioner::sourceModelReset);
    disconnect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &Positioner::sourceRowsAboutToBeInserted);
    disconnect(model, &QAbstractItemModel::rowsAboutToBeMoved, this, &Positioner::sourceRowsAboutToBeMoved);
    disconnect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &Positioner::sourceRowsAboutToBeRemoved);
    disconnect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &Positioner::sourceLayoutAboutToBeChanged);
    disconnect(model, &QAbstractItemModel::rowsInserted, this, &Positioner::sourceRowsInserted);
    disconnect(model, &QAbstractItemModel::rowsMoved, this, &Positioner::sourceRowsMoved);
    disconnect(model, &QAbstractItemModel::rowsRemoved, this, &Positioner::sourceRowsRemoved);
    disconnect(model, &QAbstractItemModel::layoutChanged, this, &Positioner::sourceLayoutChanged);
    disconnect(m_folderModel, &FolderModel::urlChanged, this, &Positioner::reset);
    disconnect(m_folderModel, &FolderModel::statusChanged, this, &Positioner::sourceStatusChanged);
}
