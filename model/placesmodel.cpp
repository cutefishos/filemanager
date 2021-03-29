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

#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageDrive>
#include <Solid/StorageAccess>
#include <Solid/Predicate>

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    const QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    if (QDir(homePath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Home"), QUrl::fromLocalFile(homePath));
        item->setIconPath("qrc:/images/folder-home.svg");
        m_items.append(item);
    }

    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (QDir(desktopPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Desktop"), QUrl::fromLocalFile(desktopPath));
        item->setIconPath("qrc:/images/folder-desktop.svg");
        m_items.append(item);
    }

    const QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (QDir(documentsPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Documents"), QUrl::fromLocalFile(documentsPath));
        item->setIconPath("qrc:/images/folder-document.svg");
        m_items.append(item);
    }

    const QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (QDir(downloadPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Downloads"), QUrl::fromLocalFile(downloadPath));
        item->setIconPath("qrc:/images/folder-download.svg");
        m_items.append(item);
    }

    const QString musicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (QDir(musicPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Music"), QUrl::fromLocalFile(musicPath));
        item->setIconPath("qrc:/images/folder-music.svg");
        m_items.append(item);
    }

    const QString picturePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (QDir(picturePath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Pictures"), QUrl::fromLocalFile(picturePath));
        item->setIconPath("qrc:/images/folder-picture.svg");
        m_items.append(item);
    }

    const QString videoPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (QDir(videoPath).exists()) {
        PlacesItem *item = new PlacesItem(tr("Videos"), QUrl::fromLocalFile(videoPath));
        item->setIconPath("qrc:/images/folder-video.svg");
        m_items.append(item);
    }

    PlacesItem *trashItem = new PlacesItem(tr("Trash"), QUrl(QStringLiteral("trash:/")));
    trashItem->setIconPath("qrc:/images/user-trash.svg");
    m_items.append(trashItem);

    Solid::Predicate predicate;
    QString predicateStr(
        QString::fromLatin1("[[[[ StorageVolume.ignored == false AND [ StorageVolume.usage == 'FileSystem' OR StorageVolume.usage == 'Encrypted' ]]"
                            " OR "
                            "[ IS StorageAccess AND StorageDrive.driveType == 'Floppy' ]]"
                            " OR "
                            "OpticalDisc.availableContent & 'Audio' ]"
                            " OR "
                            "StorageAccess.ignored == false ]"));
    predicate = Solid::Predicate::fromString(predicateStr);

    Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();
    const QList<Solid::Device> &deviceList = Solid::Device::listFromQuery(predicate);

    for (const Solid::Device &device : deviceList) {
        qDebug() << device.udi();

        PlacesItem *deviceItem = new PlacesItem(device.udi());
        m_items.append(deviceItem);
    }
}

PlacesModel::~PlacesModel()
{
}

QHash<int, QByteArray> PlacesModel::roleNames() const
{
    QHash<int, QByteArray> roleNames; // = QAbstractItemModel::roleNames();
    roleNames[PlacesModel::NameRole] = "name";
    roleNames[PlacesModel::IconNameRole] = "icon";
    roleNames[PlacesModel::IconPathRole] = "iconPath";
    roleNames[PlacesModel::UrlRole] = "url";
    roleNames[PlacesModel::PathRole] = "path";
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
        return item->displayName();
        break;
    case PlacesModel::IconNameRole:
        return item->iconName();
        break;
    case PlacesModel::IconPathRole:
        return item->iconPath();
        break;
    case PlacesModel::UrlRole:
        return item->url();
        break;
    case PlacesModel::PathRole:
        return item->path();
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
