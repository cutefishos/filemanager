// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "placesmodel.h"
#include "helper/fm.h"
#include "helper/mountsclient.h"
#include "operations/fileoperationsclient.h"

#include <fileitem.h>
#include <task.h>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>

#include <preferences.h>

static PlacesItem *createFolderItem(const QUrl &url)
{
    const QString path = url.toLocalFile();
    const QString standard = Fm::standardFolderName(path);
    auto *item = new PlacesItem(standard.isEmpty() ? QFileInfo(path).fileName() : standard,
                                url);
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
        PlacesItem *item = new PlacesItem(Fm::standardFolderName(homePath),
                                          QUrl::fromLocalFile(homePath));
        item->setIconName("folder-home");
        m_items.append(item);
    }

    const QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (QDir(desktopPath).exists()) {
        PlacesItem *item = new PlacesItem(Fm::standardFolderName(desktopPath),
                                          QUrl::fromLocalFile(desktopPath));
        item->setIconName("folder-desktop");
        m_items.append(item);
    }

    const QString documentsPath =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (QDir(documentsPath).exists()) {
        PlacesItem *item = new PlacesItem(Fm::standardFolderName(documentsPath),
                                          QUrl::fromLocalFile(documentsPath));
        item->setIconName("folder-documents");
        m_items.append(item);
    }

    const QString downloadPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (QDir(downloadPath).exists()) {
        PlacesItem *item = new PlacesItem(Fm::standardFolderName(downloadPath),
                                          QUrl::fromLocalFile(downloadPath));
        item->setIconName("folder-download");
        m_items.append(item);
    }

    const QString musicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (QDir(musicPath).exists()) {
        PlacesItem *item = new PlacesItem(Fm::standardFolderName(musicPath),
                                          QUrl::fromLocalFile(musicPath));
        item->setIconName("folder-music");
        m_items.append(item);
    }

    const QString picturePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (QDir(picturePath).exists()) {
        PlacesItem *item = new PlacesItem(Fm::standardFolderName(picturePath),
                                          QUrl::fromLocalFile(picturePath));
        item->setIconName("folder-picture");
        m_items.append(item);
    }

    const QString videoPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (QDir(videoPath).exists()) {
        PlacesItem *item = new PlacesItem(Fm::standardFolderName(videoPath),
                                          QUrl::fromLocalFile(videoPath));
        item->setIconName("folder-video");
        m_items.append(item);
    }

    QSettings settings(Preferences::file(QStringLiteral("filemanager")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Sidebar"));
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

    connect(MountsClient::instance(), &MountsClient::mountsChanged, this, &PlacesModel::syncMounts);
    syncMounts();
}

// Phones and servers have URLs for ids; disks have udisks object paths.
static bool isMountId(const QString &id)
{
    return id.contains(QLatin1String("://"));
}

void PlacesModel::syncMounts()
{
    const auto mounts = MountsClient::instance()->mounts();
    QSet<QString> present;

    for (const MountsClient::Mount &mount : mounts) {
        present.insert(mount.id);

        Cutefish::IO::Volume volume;
        volume.id = mount.id;
        volume.name = mount.name;
        volume.icon = mount.icon;
        volume.filesystem = true;
        volume.ejectable = mount.device;
        if (!mount.path.isEmpty())
            volume.mountPoints = {mount.path};

        PlacesItem *existing = nullptr;
        for (PlacesItem *item : std::as_const(m_items)) {
            if (item->udi() == mount.id)
                existing = item;
        }

        if (existing) {
            existing->setVolume(volume);
            continue;
        }

        auto *item = new PlacesItem;
        item->setVolume(volume);
        insertLocation(item, mount.network ? tr("Network") : tr("Locations"));
    }

    for (int i = m_items.size() - 1; i >= 0; --i) {
        const QString id = m_items.at(i)->udi();
        if (isMountId(id) && !present.contains(id)) {
            beginRemoveRows(QModelIndex(), i, i);
            delete m_items.takeAt(i);
            endRemoveRows();
        }
    }
}

// Sections need their rows together: Locations come before Network.
void PlacesModel::insertLocation(PlacesItem *item, const QString &category)
{
    item->setCategory(category);

    int row = m_items.size();
    if (category != tr("Network")) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items.at(i)->category() == tr("Network")) {
                row = i;
                break;
            }
        }
    }

    beginInsertRows(QModelIndex(), row, row);
    m_items.insert(row, item);
    endInsertRows();
    connect(item, &PlacesItem::itemChanged, this, &PlacesModel::onItemChanged);
}

QStringList PlacesModel::recentServers() const
{
    QSettings settings(Preferences::file(QStringLiteral("filemanager")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Network"));
    return settings.value(QStringLiteral("recentServers")).toStringList();
}

void PlacesModel::addRecentServer(const QString &address)
{
    QStringList servers = recentServers();
    servers.removeAll(address);
    servers.prepend(address);
    while (servers.size() > 10)
        servers.removeLast();

    QSettings settings(Preferences::file(QStringLiteral("filemanager")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Network"));
    settings.setValue(QStringLiteral("recentServers"), servers);
    emit recentServersChanged();
}

void PlacesModel::removeRecentServer(const QString &address)
{
    QStringList servers = recentServers();
    if (!servers.removeAll(address))
        return;

    QSettings settings(Preferences::file(QStringLiteral("filemanager")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Network"));
    settings.setValue(QStringLiteral("recentServers"), servers);
    emit recentServersChanged();
}

void PlacesModel::connectToServer(const QString &address)
{
    const QString location = MountsClient::normalizedAddress(address);
    if (location.isEmpty()) {
        emit deviceOperationFailed(tr("“%1” is not a server address. Enter one such as "
                                      "smb://server/share or sftp://user@server.")
                                           .arg(address.trimmed()));
        return;
    }

    m_connecting.insert(location, {});
    mountLocation(location);
}

void PlacesModel::mountLocation(const QString &address)
{
    QPointer<PlacesModel> self(this);
    MountsClient::instance()->requestMount(
            address, m_connecting.value(address),
            [self, address](const QString &status, const QVariantMap &details) {
                if (!self || !self->m_connecting.contains(address))
                    return;

                if (status == QLatin1String("password")) {
                    emit self->passwordRequested(address,
                                                 details.value(QStringLiteral("user")).toString(),
                                                 details.value(QStringLiteral("domain")).toString(),
                                                 details.value(QStringLiteral("needsDomain")).toBool(),
                                                 details.value(QStringLiteral("anonymousAllowed")).toBool(),
                                                 details.value(QStringLiteral("retry")).toBool());
                    return;
                }

                if (status == QLatin1String("hostkey")) {
                    emit self->hostKeyRequested(address,
                                                details.value(QStringLiteral("fingerprint")).toString(),
                                                details.value(QStringLiteral("changed")).toBool());
                    return;
                }

                self->m_connecting.remove(address);
                if (status == QLatin1String("mounted")) {
                    self->addRecentServer(address);
                    emit self->deviceSetupDone(details.value(QStringLiteral("path")).toString());
                } else {
                    emit self->deviceOperationFailed(details.value(QStringLiteral("message")).toString());
                }
            });
}

void PlacesModel::answerPassword(const QString &address, const QString &user,
                                 const QString &domain, const QString &password, bool anonymous,
                                 bool remember)
{
    if (!m_connecting.contains(address))
        return;

    QVariantMap &options = m_connecting[address];
    options.insert(QStringLiteral("user"), user);
    options.insert(QStringLiteral("domain"), domain);
    options.insert(QStringLiteral("password"), password);
    options.insert(QStringLiteral("anonymous"), anonymous);
    options.insert(QStringLiteral("remember"), remember);
    mountLocation(address);
}

void PlacesModel::acceptHostKey(const QString &address, const QString &fingerprint)
{
    if (!m_connecting.contains(address))
        return;

    m_connecting[address].insert(QStringLiteral("hostKey"), fingerprint);
    mountLocation(address);
}

void PlacesModel::cancelConnection(const QString &address)
{
    m_connecting.remove(address);
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

bool PlacesModel::canDropInto(const QList<QUrl> &urls, int row, int action) const
{
    if (urls.isEmpty() || row < 0 || row >= m_items.size())
        return false;
    const PlacesItem *item = m_items.at(row);
    const bool toTrash = item->url().scheme() == QLatin1String("trash");
    if (!toTrash) {
        const QFileInfo target(item->path());
        if (item->path().isEmpty() || !target.isDir() || !target.isWritable()
            || !target.isExecutable())
            return false;
    }
    const QString target = QDir::cleanPath(item->path());
    const auto dropAction = static_cast<Qt::DropAction>(action);
    const bool moving = toTrash || dropAction == Qt::MoveAction
            || dropAction == Qt::TargetMoveAction;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            return false;
        if (moving && !Cutefish::IO::canMoveStandardFolderTo(url, item->url()))
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
    if (!canDropInto(urls, row, action))
        return false;
    const PlacesItem *item = m_items.at(row);
    if (item->url().scheme() == QLatin1String("trash"))
        FileOperationsClient::instance()->trash(urls);
    else if (action == Qt::CopyAction)
        FileOperationsClient::instance()->copy(urls, QUrl::fromLocalFile(item->path()));
    else
        FileOperationsClient::instance()->move(urls, QUrl::fromLocalFile(item->path()));
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
    QSettings settings(Preferences::file(QStringLiteral("filemanager")), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Sidebar"));
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
    if (index < 0 || index >= m_items.size() || !m_items.at(index)->isDevice())
        return;

    const QString id = m_items.at(index)->udi();
    if (!isMountId(id)) {
        m_volumeManager->mount(id);
        return;
    }

    QPointer<PlacesModel> self(this);
    MountsClient::instance()->requestMount(id, {}, [self](const QString &status,
                                                          const QVariantMap &details) {
        if (!self)
            return;
        if (status == QLatin1String("mounted"))
            emit self->deviceSetupDone(details.value(QStringLiteral("path")).toString());
        else
            emit self->deviceOperationFailed(details.value(QStringLiteral("message")).toString());
    });
}

static void unmountLocation(PlacesModel *model, const QString &id)
{
    QPointer<PlacesModel> self(model);
    MountsClient::instance()->requestUnmount(id, [self](const QString &error) {
        if (!self || error.isEmpty())
            return;
        // fusermount refuses while anything has files open there.
        emit self->deviceOperationFailed(error.contains(QLatin1String("busy"))
                                                 ? PlacesModel::tr("It is in use. Close the files "
                                                                   "open there and try again.")
                                                 : error);
    });
}

void PlacesModel::requestEject(const int &index)
{
    if (index < 0 || index >= m_items.size() || !m_items.at(index)->isDevice())
        return;

    const QString id = m_items.at(index)->udi();
    if (isMountId(id))
        unmountLocation(this, id);
    else
        m_volumeManager->eject(id);
}

void PlacesModel::requestTeardown(const int &index)
{
    if (index < 0 || index >= m_items.size() || !m_items.at(index)->isDevice())
        return;

    const QString id = m_items.at(index)->udi();
    if (isMountId(id))
        unmountLocation(this, id);
    else
        m_volumeManager->unmount(id);
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
    insertLocation(item, tr("Locations"));
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
    int index = m_items.indexOf(item);

    if (index < 0 || index > m_items.size())
        return;

    QModelIndex idx = this->index(index, 0);
    emit dataChanged(idx, idx);
}
