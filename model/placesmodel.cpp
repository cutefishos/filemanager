/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     revenmartin <revenmartin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "placesmodel.h"

#include <fileitem.h>
#include <task.h>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

static PlacesItem *createFolderItem(const QUrl &url)
{
    const QString path = url.toLocalFile();
    auto *item = new PlacesItem(QFileInfo(path).fileName(), url);
    const QString icon = Cutefish::IO::standardFolderIconName(path);
    item->setIconName(icon.isEmpty() ? QStringLiteral("folder") : icon);
    return item;
}

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_volumeManager(new Cutefish::IO::VolumeManager(this))
{
    const QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    if (QDir(homePath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Home"), QUrl::fromLocalFile(homePath));
        item->setIconName("folder-home");
        m_items.append(item);
    }

    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (QDir(desktopPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Desktop"), QUrl::fromLocalFile(desktopPath));
        item->setIconName("folder-desktop");
        m_items.append(item);
    }

    const QString documentsPath =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (QDir(documentsPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Documents"), QUrl::fromLocalFile(documentsPath));
        item->setIconName("folder-documents");
        m_items.append(item);
    }

    const QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (QDir(downloadPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Downloads"), QUrl::fromLocalFile(downloadPath));
        item->setIconName("folder-download");
        m_items.append(item);
    }

    const QString musicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (QDir(musicPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Music"), QUrl::fromLocalFile(musicPath));
        item->setIconName("folder-music");
        m_items.append(item);
    }

    const QString picturePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (QDir(picturePath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Pictures"), QUrl::fromLocalFile(picturePath));
        item->setIconName("folder-picture");
        m_items.append(item);
    }

    const QString videoPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (QDir(videoPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Videos"), QUrl::fromLocalFile(videoPath));
        item->setIconName("folder-video");
        m_items.append(item);
    }

    QSettings settings(QStringLiteral("cutefish"), QStringLiteral("filemanager-sidebar"));
    if (settings.contains(QStringLiteral("favorites"))) {
        const auto defaults = m_items;
        m_items.clear();
        const QStringList saved = settings.value(QStringLiteral("favorites")).toStringList();
        for (const QString &entry : saved) {
            const QUrl url(entry);
            if (!url.isLocalFile())
                continue;
            PlacesItem *favorite = nullptr;
            for (PlacesItem *item : defaults) {
                if (item->url() == url) {
                    favorite = new PlacesItem(item->displayName(), url);
                    favorite->setIconName(item->iconName());
                    break;
                }
            }
            if (!favorite)
                favorite = createFolderItem(url);
            m_items.append(favorite);
        }
        qDeleteAll(defaults);
    }
    for (PlacesItem *item : m_items)
        item->setCategory(tr("Favorites"));

    connect(m_volumeManager, &Cutefish::IO::VolumeManager::volumeAdded, this,
            &PlacesModel::onDeviceAdded);
    connect(m_volumeManager, &Cutefish::IO::VolumeManager::volumeRemoved, this,
            &PlacesModel::onDeviceRemoved);
    connect(m_volumeManager, &Cutefish::IO::VolumeManager::volumeChanged, this,
            [this](const QString &id) {
                onDeviceAdded(id);
            });
    connect(m_volumeManager, &Cutefish::IO::VolumeManager::operationFinished, this,
            [this](const QString &id, const QString &operation, const QString &path) {
                if (operation != "mount" || path.isEmpty())
                    return;
                for (PlacesItem *item : m_items) {
                    if (item->udi() == id) {
                        auto volume = m_volumeManager->volume(id);
                        volume.mountPoints = {path};
                        item->setVolume(volume);
                        emit deviceSetupDone(path);
                        break;
                    }
                }
            });
    connect(m_volumeManager, &Cutefish::IO::VolumeManager::operationFailed, this,
            [this](const QString &, const QString &, const QString &message) {
                emit deviceOperationFailed(message);
            });

    PlacesItem *trashItem = new PlacesItem(tr("Trash"), QUrl(QStringLiteral("trash:///")));
    trashItem->setIconName("user-trash");
    m_items.append(trashItem);

    // Init Signals
    for (PlacesItem *item : m_items) {
        connect(item, &PlacesItem::itemChanged, this, &PlacesModel::onItemChanged);
    }
}

PlacesModel::~PlacesModel()
{
    qDeleteAll(m_items);
}

int PlacesModel::favoriteCount() const
{
    int count = 0;
    while (count < m_items.size() && m_items.at(count)->category() == tr("Favorites"))
        ++count;
    return count;
}

bool PlacesModel::canAddFavorites(const QList<QUrl> &urls) const
{
    if (urls.isEmpty())
        return false;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir())
            return false;
    }
    return true;
}

bool PlacesModel::addFavorites(const QList<QUrl> &urls, int before)
{
    if (before < 0 || before > favoriteCount() || !canAddFavorites(urls))
        return false;
    for (const QUrl &input : urls) {
        const QUrl url = QUrl::fromLocalFile(QDir::cleanPath(input.toLocalFile()));
        bool duplicate = false;
        for (int row = 0; row < favoriteCount(); ++row)
            duplicate |= m_items.at(row)->url() == url;
        if (duplicate)
            continue;
        auto *item = createFolderItem(url);
        item->setCategory(tr("Favorites"));
        beginInsertRows(QModelIndex(), before, before);
        m_items.insert(before++, item);
        endInsertRows();
    }
    saveFavorites();
    return true;
}

bool PlacesModel::moveFavorite(int from, int before)
{
    const int count = favoriteCount();
    if (from < 0 || from >= count || before < 0 || before > count)
        return false;
    if (before == from || before == from + 1)
        return true;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), before);
    m_items.move(from, before > from ? before - 1 : before);
    endMoveRows();
    saveFavorites();
    return true;
}

bool PlacesModel::canDropInto(const QList<QUrl> &urls, int row) const
{
    if (urls.isEmpty() || row < 0 || row >= m_items.size())
        return false;
    const PlacesItem *item = m_items.at(row);
    const bool toTrash = item->url().scheme() == QLatin1String("trash");
    if (!toTrash && (item->path().isEmpty() || !QFileInfo(item->path()).isWritable()))
        return false;
    const QString target = QDir::cleanPath(item->path());
    for (const QUrl &url : urls) {
        if (!url.isLocalFile() || Cutefish::IO::isStandardFolder(url))
            return false;
        if (toTrash)
            continue;
        const QString source = QDir::cleanPath(url.toLocalFile());
        // Dropping into the parent is a no-op; into itself or a descendant is impossible.
        if (QFileInfo(source).absolutePath() == target || target == source
            || target.startsWith(source + QLatin1Char('/')))
            return false;
    }
    return true;
}

bool PlacesModel::dropInto(const QList<QUrl> &urls, int row, int action)
{
    if (!canDropInto(urls, row))
        return false;
    const PlacesItem *item = m_items.at(row);
    if (item->url().scheme() == QLatin1String("trash"))
        Cutefish::IO::trash(urls);
    else if (action == Qt::CopyAction)
        Cutefish::IO::copy(urls, QUrl::fromLocalFile(item->path()));
    else
        Cutefish::IO::move(urls, QUrl::fromLocalFile(item->path()));
    return true;
}

void PlacesModel::removeFavorite(int row)
{
    if (row < 0 || row >= favoriteCount())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    delete m_items.takeAt(row);
    endRemoveRows();
    saveFavorites();
}

void PlacesModel::saveFavorites()
{
    QStringList urls;
    for (int row = 0; row < favoriteCount(); ++row)
        urls.append(m_items.at(row)->url().toString());
    QSettings settings(QStringLiteral("cutefish"), QStringLiteral("filemanager-sidebar"));
    settings.setValue(QStringLiteral("favorites"), urls);
    emit favoritesChanged();
}

QHash<int, QByteArray> PlacesModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[PlacesModel::NameRole] = "name";
    roleNames[PlacesModel::IconNameRole] = "iconName";
    roleNames[PlacesModel::UrlRole] = "url";
    roleNames[PlacesModel::PathRole] = "path";
    roleNames[PlacesModel::IsDeviceRole] = "isDevice";
    roleNames[PlacesModel::IsOpticalDisc] = "isOpticalDisc";
    roleNames[PlacesModel::setupNeededRole] = "setupNeeded";
    roleNames[PlacesModel::CanEjectRole] = "canEject";
    roleNames[PlacesModel::CategoryRole] = "category";
    return roleNames;
}

int PlacesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_items.size();
}

int PlacesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant PlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    PlacesItem *item = m_items.at(index.row());

    switch (role) {
    case PlacesModel::NameRole:
        return item->url().toLocalFile() == QDir::rootPath() ? tr("Computer") : item->displayName();
        break;
    case PlacesModel::IconNameRole:
        return item->iconName();
        break;
    case PlacesModel::UrlRole:
        return item->url();
        break;
    case PlacesModel::PathRole:
        return item->path();
        break;
    case PlacesModel::IsDeviceRole:
        return item->isDevice();
        break;
    case PlacesModel::IsOpticalDisc:
        return item->isOpticalDisc();
        break;
    case PlacesModel::setupNeededRole:
        return item->setupNeeded();
        break;
    case PlacesModel::CanEjectRole:
        return item->canEject();
    case PlacesModel::CategoryRole:
        return item->category();
        break;
    default:
        break;
    }

    return QVariant();
}

QModelIndex PlacesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0 || row >= m_items.size()) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        return QModelIndex();
    }

    return createIndex(row, column, m_items.at(row));
}

QModelIndex PlacesModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);

    return QModelIndex();
}

QVariantMap PlacesModel::get(const int &index) const
{
    QVariantMap res;

    if (index >= this->rowCount() || index < 0)
        return res;

    const auto roleNames = this->roleNames();

    for (auto i = roleNames.begin(); i != roleNames.end(); ++i) {
        res.insert(i.value(), this->index(index, 0).data(i.key()).toString());
    }

    return res;
}

void PlacesModel::requestSetup(const int &index)
{
    if (index >= 0 && index < m_items.size() && m_items.at(index)->isDevice())
        m_volumeManager->mount(m_items.at(index)->udi());
}

void PlacesModel::requestEject(const int &index)
{
    if (index >= 0 && index < m_items.size() && m_items.at(index)->isDevice())
        m_volumeManager->eject(m_items.at(index)->udi());
}

void PlacesModel::requestTeardown(const int &index)
{
    if (index >= 0 && index < m_items.size() && m_items.at(index)->isDevice())
        m_volumeManager->unmount(m_items.at(index)->udi());
}

void PlacesModel::onDeviceAdded(const QString &udi)
{
    const auto volume = m_volumeManager->volume(udi);
    if (!volume.filesystem && !volume.optical) {
        onDeviceRemoved(udi);
        return;
    }
    for (PlacesItem *item : m_items) {
        if (item->udi() == udi) {
            item->setVolume(volume);
            return;
        }
    }
    auto *item = new PlacesItem;
    item->setVolume(volume);
    item->setCategory(tr("Locations"));
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_items.append(item);
    endInsertRows();
    connect(item, &PlacesItem::itemChanged, this, &PlacesModel::onItemChanged);
}

void PlacesModel::onDeviceRemoved(const QString &udi)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i)->udi() == udi) {
            beginRemoveRows(QModelIndex(), i, i);
            PlacesItem *item = m_items.at(i);
            m_items.removeOne(item);
            endRemoveRows();

            delete item;
            return;
        }
    }
}

void PlacesModel::onItemChanged(PlacesItem *item)
{
    // 更新 item 数据
    int index = m_items.indexOf(item);

    if (index < 0 || index > m_items.size())
        return;

    QModelIndex idx = this->index(index, 0);
    emit dataChanged(idx, idx);
}
