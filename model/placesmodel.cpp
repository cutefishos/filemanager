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

#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>

#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageDrive>
#include <Solid/StorageAccess>
#include <Solid/Predicate>
#include <Solid/OpticalDrive>

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractItemModel(parent)
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

    const QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
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
            if (!favorite) {
                favorite = new PlacesItem(QFileInfo(url.toLocalFile()).fileName(), url);
                favorite->setIconName(QStringLiteral("folder"));
            }
            m_items.append(favorite);
        }
        qDeleteAll(defaults);
    }
    for (PlacesItem *item : m_items)
        item->setCategory(tr("Favorites"));

    QString predicateStr(
        QString::fromLatin1("[[[[ StorageVolume.ignored == false AND [ StorageVolume.usage == 'FileSystem' OR StorageVolume.usage == 'Encrypted' ]]"
                            " OR "
                            "[ IS StorageAccess AND StorageDrive.driveType == 'Floppy' ]]"
                            " OR "
                            "OpticalDisc.availableContent & 'Audio' ]"
                            " OR "
                            "StorageAccess.ignored == false ]"));
    m_predicate = Solid::Predicate::fromString(predicateStr);

    Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();
    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, &PlacesModel::onDeviceAdded);
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this, &PlacesModel::onDeviceRemoved);

    // Init devices
    const QList<Solid::Device> &deviceList = Solid::Device::listFromQuery(m_predicate);
    for (const Solid::Device &device : deviceList) {
        PlacesItem *deviceItem = new PlacesItem;
        deviceItem->setUdi(device.udi());
        deviceItem->setCategory(tr("Locations"));
        m_items.append(deviceItem);
    }

    PlacesItem *trashItem = new PlacesItem(tr("Trash"), QUrl(QStringLiteral("trash:///")));
    trashItem->setCategory(tr("Locations"));
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
        auto *item = new PlacesItem(QFileInfo(url.toLocalFile()).fileName(), url);
        item->setIconName(QStringLiteral("folder"));
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
    PlacesItem *item = m_items.at(index);
    if (!item->udi().isEmpty()) {
        Solid::Device device = Solid::Device(item->udi());
        Solid::StorageAccess *access = device.as<Solid::StorageAccess>();
        access->setup();
        connect(access, &Solid::StorageAccess::setupDone, this, [this, item, access]() {
            if (item) {
                // 更新信息，让 qml 里的 sidebar 识别到
                item->setUrl(QUrl::fromLocalFile(access->filePath()));
                emit deviceSetupDone(access->filePath());
            }
        });
    }
}

void PlacesModel::requestEject(const int &index)
{
    PlacesItem *item = m_items.at(index);
    if (!item->udi().isEmpty()) {
        Solid::Device device = Solid::Device(item->udi());
        Solid::OpticalDrive *drive = device.parent().as<Solid::OpticalDrive>();

        if (drive) {
            drive->eject();
        }
    }
}

void PlacesModel::requestTeardown(const int &index)
{
    PlacesItem *item = m_items.at(index);

    if (!item->udi().isEmpty()) {
        Solid::Device device = Solid::Device(item->udi());
        Solid::StorageAccess *access = device.as<Solid::StorageAccess>();

        if (access != nullptr) {
            access->teardown();
        }
    }
}

void PlacesModel::onDeviceAdded(const QString &udi)
{
    if (m_predicate.matches(Solid::Device(udi))) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        PlacesItem *deviceItem = new PlacesItem;
        deviceItem->setUdi(udi);
        deviceItem->setCategory(tr("Locations"));
        m_items.append(deviceItem);
        endInsertRows();

        connect(deviceItem, &PlacesItem::itemChanged, this, &PlacesModel::onItemChanged);
    }
}

void PlacesModel::onDeviceRemoved(const QString &udi)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i)->udi() == udi) {
            beginRemoveRows(QModelIndex(), i, i);
            PlacesItem *item = m_items.at(i);
            m_items.removeOne(item);
            endRemoveRows();

            disconnect(item);
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
