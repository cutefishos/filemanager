#include "foldercontentsmodel.h"
#include "helper/fm.h"

#include <backgroundrelease.h>

#include <QCollator>
#include <QFutureWatcher>
#include <QLocale>
#include <QSet>
#include <QTimeZone>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cerrno>
#include <numeric>
#include <vector>

using Cutefish::IO::DirectoryModel;
using Cutefish::IO::FileItem;
using Cutefish::IO::FileItemList;
using Cutefish::IO::releaseInBackground;

namespace {
constexpr int BatchSize = 256;

bool sameMetadata(const FileItem &left, const FileItem &right)
{
    // Equal stamps answer for size, kind and modification time without the accessors.
    const auto sameStat = [&] {
        return left.sameStat(right)
                || (left.size() == right.size() && left.isDir() == right.isDir()
                    && left.isLink() == right.isLink()
                    && left.time(FileItem::ModificationTime, QTimeZone::UTC)
                            == right.time(FileItem::ModificationTime, QTimeZone::UTC));
    };

    return left.url() == right.url() && left.text() == right.text() && sameStat()
            && left.isHidden() == right.isHidden() && left.isWritable() == right.isWritable()
            && left.mimetype() == right.mimetype() && left.iconName() == right.iconName()
            && left.targetUrl() == right.targetUrl()
            && left.typeDescription() == right.typeDescription();
}

// What the sort compares, read once per item instead of once per comparison.
struct SortEntry
{
    QString name;
    QString mimeType;
    QDateTime modified;
    quint64 size = 0;
    bool dir = false;
};
}

struct FolderContentsModel::Prepared
{
    FileItemList ordered;
    FileItemList retained;
    FileItemList added;
    QHash<QUrl, int> rows;
    QList<QPair<int, int>> removals;
    QSet<int> changed;
    QList<QUrl> consumedRenames;
    bool reorder = false;
};

FolderContentsModel::FolderContentsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_prepareTimer.setSingleShot(true);
    m_publishTimer.setSingleShot(true);
    m_publishTimer.setInterval(1);
    connect(&m_prepareTimer, &QTimer::timeout, this, &FolderContentsModel::prepare);
    connect(&m_publishTimer, &QTimer::timeout, this, &FolderContentsModel::publishBatch);
}

FolderContentsModel::~FolderContentsModel()
{
    cancelPreparation();
}

int FolderContentsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

FileItem FolderContentsModel::snapshotItem(const QModelIndex &index) const
{
    return index.model() == this && index.isValid() && index.row() < m_items.size()
            ? m_items.at(index.row()) : FileItem();
}

// The item a row past the end will hold once the insertion announced by
// rowsAboutToBeInserted finishes; null outside that window.

FileItem FolderContentsModel::pendingItem(int row) const
{
    if (!m_prepared || row < m_items.size())
        return FileItem();

    const int offset = m_insertOffset + (row - m_items.size());

    return offset < m_prepared->added.size() ? m_prepared->added.at(offset) : FileItem();
}

QVariant FolderContentsModel::data(const QModelIndex &index, int role) const
{
    const auto item = snapshotItem(index);
    if (item.isNull())
        return {};

    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return item.name();

    if (role == DirectoryModel::FileItemRole)
        return QVariant::fromValue(item);

    return {};
}

bool FolderContentsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    const auto source = mapToSource(index);

    return source.isValid() && m_source->setData(source, value, role);
}

Qt::ItemFlags FolderContentsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;

    const auto item = snapshotItem(index);
    if (item.isNull())
        return Qt::NoItemFlags;

    auto result = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
            | Qt::ItemIsEditable;
    if (item.isDir())
        result |= Qt::ItemIsDropEnabled;
    return result;
}

QMimeData *FolderContentsModel::mimeData(const QModelIndexList &indexes) const
{
    auto *mime = new QMimeData;
    QList<QUrl> urls;
    QSet<QUrl> seen;

    for (const auto &index : indexes) {
        const auto item = snapshotItem(index);
        if (!item.isNull() && !seen.contains(item.url())) {
            seen.insert(item.url());
            urls.append(item.url());
        }
    }

    mime->setUrls(urls);

    return mime;
}

QStringList FolderContentsModel::mimeTypes() const
{
    return {QStringLiteral("text/uri-list")};
}

QModelIndex FolderContentsModel::viewIndexForUrl(const QUrl &url) const
{
    if (m_rowsValid)
        return index(m_rowsByUrl.value(url, -1), 0);

    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row].url() == url)
            return index(row, 0);
    }

    return {};
}

QModelIndex FolderContentsModel::mapToSource(const QModelIndex &index) const
{
    const auto item = snapshotItem(index);

    return m_source && !item.isNull() ? m_source->indexForUrl(item.url()) : QModelIndex();
}

QModelIndex FolderContentsModel::mapFromSource(const QModelIndex &index) const
{
    return m_source && index.model() == m_source
            ? viewIndexForUrl(m_source->itemForIndex(index).url()) : QModelIndex();
}

void FolderContentsModel::setSourceModel(DirectoryModel *source)
{
    if (m_source == source)
        return;

    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
        disconnect(m_source->dirLister(), nullptr, this, nullptr);
    }

    clear();
    m_source = source;

    if (!source)
        return;

    connect(source, &QAbstractItemModel::modelReset, this, &FolderContentsModel::clear);
    connect(source->dirLister(), &Cutefish::IO::DirectoryLister::itemsChanged,
            this, &FolderContentsModel::schedulePreparation);
    connect(source, &QObject::destroyed, this, [this] {
        m_source = nullptr;
        clear();
    });

    if (!source->items().isEmpty())
        schedulePreparation();
}

void FolderContentsModel::setQuery(const FolderQuery &query)
{
    m_query = query;
    schedulePreparation();
}

void FolderContentsModel::cancelPreparation()
{
    if (m_cancelled)
        m_cancelled->store(true);

    m_prepareTimer.stop();
    m_publishTimer.stop();
    releaseInBackground(std::exchange(m_prepared, {}));
    releaseInBackground(std::exchange(m_released, {}));
}

void FolderContentsModel::clear()
{
    cancelPreparation();
    beginResetModel();
    releaseInBackground(std::exchange(m_items, {}));
    releaseInBackground(std::exchange(m_rowsByUrl, {}));
    m_renames.clear();
    m_rowsValid = true;
    endResetModel();
}

// Lets the next listing treat `from` reappearing as `to` as the same row.

void FolderContentsModel::expectRename(const QUrl &from, const QUrl &to)
{
    m_renames.insert(from, to);
}

void FolderContentsModel::schedulePreparation()
{
    cancelPreparation();

    if (m_source && !m_source->dirLister()->isListing())
        m_prepareTimer.start();
}

void FolderContentsModel::prepare()
{
    if (!m_source || m_source->dirLister()->isListing())
        return;

    const auto cancelled = std::make_shared<std::atomic_bool>(false);
    m_cancelled = cancelled;

    auto *watcher = new QFutureWatcher<std::shared_ptr<Prepared>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<Prepared>>::finished, this,
            [this, watcher, cancelled, offered = m_renames.keys()] {
        watcher->deleteLater();

        if (cancelled->load())
            return;

        m_prepared = watcher->result();
        // A rename this listing did not match had its chance; only ones
        // expected since it started stay.
        for (const QUrl &from : offered)
            m_renames.remove(from);
        m_updateOffset = m_prepared->changed.isEmpty() ? m_prepared->retained.size() : 0;
        m_insertOffset = 0;
        m_publishTimer.start();
    });

    watcher->setFuture(QtConcurrent::run(
            [incoming = m_source->items(), previous = m_items, query = m_query,
             renames = m_renames,
             locale = QLocale(), standardNames = Fm::standardFolderNames(), cancelled] {
        auto result = std::make_shared<Prepared>();
        auto checkpoint = [&] {
            if (cancelled->load())
                throw Cutefish::IO::Failure(ECANCELED, {});
        };

        try {
            result->ordered = incoming;

            // Rows by URL, numbered in listing order until the sort renumbers them.
            result->rows.reserve(incoming.size());
            for (int row = 0; row < incoming.size(); ++row) {
                checkpoint();
                result->rows.insert(incoming[row].url(), row);
            }

            // Names that are not UTF-8 can decode to one URL; the row of such a URL
            // is its last file's in the final order.
            const bool unique = result->rows.size() == incoming.size();

            // Each previous row's file in the listing, or -1; hashing URLs is the cost
            // here, so the sort and the comparison below share one lookup.
            std::vector<int> previousRows;
            if (unique) {
                previousRows.reserve(previous.size());
                for (const auto &item : previous) {
                    checkpoint();
                    previousRows.push_back(result->rows.value(item.url(), -1));
                }
            }

            if (query.sortColumn >= 0) {
                QCollator collator(locale);
                std::vector<SortEntry> entries(incoming.size());

                for (qsizetype i = 0; i < incoming.size(); ++i) {
                    checkpoint();
                    const auto &item = incoming[i];
                    auto &entry = entries[i];
                    entry.dir = item.isDir();
                    // Standard folders sort by the name they are shown under.
                    entry.name = entry.dir && item.url().isLocalFile()
                            ? standardNames.value(item.url().toLocalFile(), item.name())
                            : item.name();

                    if (query.sortColumn == DirectoryModel::Size)
                        entry.size = item.size();
                    else if (query.sortColumn == DirectoryModel::ModifiedTime)
                        entry.modified = item.time(FileItem::ModificationTime, QTimeZone::UTC);
                    else if (query.sortColumn == DirectoryModel::Type)
                        entry.mimeType = item.mimetype();
                }

                const auto less = [&](qsizetype a, qsizetype b) {
                    checkpoint();
                    const auto &left = entries[a];
                    const auto &right = entries[b];

                    if ((query.directoriesFirst || query.sortColumn == DirectoryModel::Size)
                        && left.dir != right.dir)
                        return left.dir;

                    int order = 0;
                    if (query.sortColumn == DirectoryModel::Size && !left.dir) {
                        order = left.size < right.size ? -1 : left.size > right.size;
                    } else if (query.sortColumn == DirectoryModel::ModifiedTime) {
                        order = left.modified < right.modified ? -1 : left.modified > right.modified;
                    } else if (query.sortColumn == DirectoryModel::Type) {
                        order = QString::compare(left.mimeType, right.mimeType);
                    }
                    if (order == 0)
                        order = collator.compare(left.name, right.name);
                    if (order == 0)
                        order = QString::compare(incoming[a].url().toString(),
                                                 incoming[b].url().toString());
                    return query.descending ? order > 0 : order < 0;
                };

                // A refresh usually keeps the files it had in their order. Then only the
                // new ones are sorted, and merged in; with the URL breaking every tie,
                // the order is the one a full sort gives.
                std::vector<qsizetype> positions;
                if (unique && !previous.isEmpty()) {
                    std::vector<qsizetype> kept;
                    kept.reserve(previous.size());
                    std::vector<bool> seen(incoming.size(), false);

                    for (const int row : previousRows) {
                        if (row >= 0 && !seen[row]) {
                            seen[row] = true;
                            kept.push_back(row);
                        }
                    }

                    bool inOrder = true;
                    for (size_t i = 1; i < kept.size() && inOrder; ++i)
                        inOrder = less(kept[i - 1], kept[i]);

                    if (inOrder) {
                        std::vector<qsizetype> fresh;
                        for (qsizetype i = 0; i < incoming.size(); ++i) {
                            if (!seen[i])
                                fresh.push_back(i);
                        }

                        std::stable_sort(fresh.begin(), fresh.end(), less);
                        positions.resize(incoming.size());
                        std::merge(kept.cbegin(), kept.cend(), fresh.cbegin(), fresh.cend(),
                                   positions.begin(), less);
                    }
                }

                if (positions.empty()) {
                    positions.resize(incoming.size());
                    std::iota(positions.begin(), positions.end(), 0);
                    std::stable_sort(positions.begin(), positions.end(), less);
                }

                FileItemList sorted;
                sorted.reserve(incoming.size());
                for (const qsizetype position : positions)
                    sorted.append(incoming[position]);
                result->ordered = std::move(sorted);

                if (unique) {
                    std::vector<int> sortedRows(incoming.size());
                    for (size_t row = 0; row < positions.size(); ++row)
                        sortedRows[positions[row]] = int(row);

                    for (auto it = result->rows.begin(); it != result->rows.end(); ++it)
                        it.value() = sortedRows[it.value()];
                    for (int &row : previousRows) {
                        if (row >= 0)
                            row = sortedRows[row];
                    }
                } else {
                    result->rows.clear();

                    for (int row = 0; row < result->ordered.size(); ++row) {
                        checkpoint();
                        result->rows.insert(result->ordered[row].url(), row);
                    }
                }
            }

            // Which rows of ordered a previous row kept; with colliding URLs, by the
            // row that owns the URL.
            std::vector<bool> retainedRows(result->ordered.size(), false);

            for (int row = 0; row < previous.size(); ++row) {
                checkpoint();
                const auto &item = previous[row];
                int found = unique ? previousRows[row] : result->rows.value(item.url(), -1);
                if (found < 0 && renames.contains(item.url())) {
                    // A rename we made ourselves: the file went away under its old
                    // name and is back under the new one. Keep the row so the
                    // view updates the icon in place instead of tearing it down.
                    result->consumedRenames.append(item.url());
                    const int renamed = result->rows.value(renames.value(item.url()), -1);
                    if (renamed >= 0 && !retainedRows[renamed])
                        found = renamed;
                }
                if (found < 0) {
                    if (!result->removals.isEmpty() && result->removals.last().second == row - 1)
                        result->removals.last().second = row;
                    else
                        result->removals.append({row, row});
                    continue;
                }

                const auto &updated = result->ordered[found];

                if (!sameMetadata(item, updated))
                    result->changed.insert(result->retained.size());

                result->retained.append(updated);
                retainedRows[found] = true;
            }

            for (int row = 0; row < result->ordered.size(); ++row) {
                checkpoint();
                const auto &item = result->ordered[row];

                if (!retainedRows[unique ? row : result->rows.value(item.url())])
                    result->added.append(item);
            }

            // Unsorted desktop views retain their current row order when files arrive.
            if (query.sortColumn < 0) {
                result->ordered = result->retained;
                result->ordered.append(result->added);
                result->rows.clear();

                for (int row = 0; row < result->ordered.size(); ++row)
                    result->rows.insert(result->ordered[row].url(), row);
            } else {
                for (int row = 0; row < result->ordered.size(); ++row) {
                    const auto &current = row < result->retained.size()
                            ? result->retained[row] : result->added[row - result->retained.size()];
                    if (current.url() != result->ordered[row].url()) {
                        result->reorder = true;
                        break;
                    }
                }
            }
        } catch (const Cutefish::IO::Failure &) {
            return std::shared_ptr<Prepared>();
        }

        return result;
    }));

    emit preparationStarted();
}

void FolderContentsModel::publishBatch()
{
    auto result = m_prepared;
    const auto cancelled = m_cancelled;

    if (!result || cancelled->load())
        return;

    if (!result->removals.isEmpty()) {
        auto &range = result->removals.last();
        const int last = range.second;
        const int first = qMax(range.first, last - BatchSize + 1);

        beginRemoveRows({}, first, last);
        for (int row = first; row <= last; ++row)
            m_released.append(std::move(m_items[row]));
        m_items.remove(first, last - first + 1);
        m_rowsValid = false;
        endRemoveRows();

        range.second = first - 1;
        if (range.second < range.first)
            result->removals.removeLast();
    } else if (m_updateOffset < result->retained.size()) {
        const int first = m_updateOffset;
        const int end = qMin(first + BatchSize, int(result->retained.size()));
        int changedFirst = -1;
        int changedLast = -1;

        for (int row = first; row < end; ++row) {
            if (m_rowsValid && m_items[row].url() != result->retained[row].url()) {
                m_rowsByUrl.remove(m_items[row].url());
                m_rowsByUrl.insert(result->retained[row].url(), row);
            }
            m_released.append(std::exchange(m_items[row], result->retained[row]));
            if (result->changed.contains(row)) {
                if (changedFirst < 0)
                    changedFirst = row;
                changedLast = row;
            }
        }

        m_updateOffset = end;
        if (changedFirst >= 0)
            emit dataChanged(index(changedFirst, 0), index(changedLast, 0));
    } else if (m_insertOffset < result->added.size()) {
        const int count = qMin(BatchSize, int(result->added.size()) - m_insertOffset);
        const int first = m_items.size();

        beginInsertRows({}, first, first + count - 1);

        for (int i = 0; i < count; ++i) {
            const auto &item = result->added[m_insertOffset + i];
            if (m_rowsValid)
                m_rowsByUrl.insert(item.url(), m_items.size());
            m_items.append(item);
        }

        m_insertOffset += count;
        endInsertRows();
    } else {
        if (result->reorder) {
            emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
            const auto from = persistentIndexList();
            QModelIndexList to;
            to.reserve(from.size());

            for (const auto &old : from)
                to.append(index(result->rows.value(m_items[old.row()].url()), 0));
            releaseInBackground(std::exchange(m_items, result->ordered));
            releaseInBackground(std::exchange(m_rowsByUrl, result->rows));
            m_rowsValid = true;
            changePersistentIndexList(from, to);
            emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
        } else {
            releaseInBackground(std::exchange(m_rowsByUrl, result->rows));
            m_rowsValid = true;
        }
        releaseInBackground(std::exchange(m_released, {}));
        if (!cancelled->load()) {
            m_prepared.reset();
            emit preparationFinished();
        }
        releaseInBackground(std::move(result));
        return;
    }
    if (!cancelled->load())
        m_publishTimer.start();
}
