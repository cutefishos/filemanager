/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2018  camilo <email>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "placeslist.h"
#include "fm.h"

#include <QEventLoop>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QTimer>

#include <KFilePlacesModel>

PlacesList::PlacesList(QObject *parent)
    : BaseList(parent)
    , fm(new FM(this))
    , model(new KFilePlacesModel(this))
    , watcher(new QFileSystemWatcher(this))
{
    /*
     *  The watcher signal returns a local file URL withouth a scheme, and the model is using a local file URL with file:// scheme.
     *  So those need to be correctly mapped
     * */
    connect(watcher, &QFileSystemWatcher::directoryChanged, [&](const QString &path) {
        if (this->count.contains(QUrl::fromLocalFile(path).toString())) {
            const auto oldCount = this->count[QUrl::fromLocalFile(path).toString()];
            const auto index = this->indexOf(FMH::MODEL_KEY::PATH, QUrl::fromLocalFile(path).toString());
            const QDir dir(path);
            const auto newCount = dir.count();
            int count = newCount - oldCount;

            this->list[index][FMH::MODEL_KEY::COUNT] = QString::number(std::max(0, count));
            emit this->updateModel(index, {FMH::MODEL_KEY::COUNT});
        }
    });

    connect(this->model, &KFilePlacesModel::reloaded, [this]() {
        this->setList();
    });

    connect(this->model, &KFilePlacesModel::rowsInserted, [this](const QModelIndex, int, int) {
        this->setList();
        emit this->bookmarksChanged();

        /*emit this->preListChanged();

        for (int i = first; i <= last; i++)
        {
            const QModelIndex index = model->index(i, 0);

            if(this->groups.contains(model->groupType(index)))
            {
                this->list << getGroup(*this->model, static_cast<FMH::PATHTYPE_KEY>(model->groupType(index)));
            }
        }
        emit this->postListChanged();	*/
    }); // TODO improve the usage of the model
}

void PlacesList::watchPath(const QString &path)
{
    if (path.isEmpty() || !FMH::fileExists(path) || !QUrl(path).isLocalFile())
        return;

    this->watcher->addPath(QUrl(path).toLocalFile());
}

void PlacesList::componentComplete()
{
    connect(this, &PlacesList::groupsChanged, this, &PlacesList::setList);
    this->setList();
}

const FMH::MODEL_LIST &PlacesList::items() const
{
    return this->list;
}

FMH::MODEL_LIST PlacesList::getGroup(const KFilePlacesModel &model, const FMH::PATHTYPE_KEY &type)
{
    FMH::MODEL_LIST res;

    if (type == FMH::PATHTYPE_KEY::QUICK_PATH) {
        res << FMH::MODEL {{FMH::MODEL_KEY::PATH, FMH::PATHTYPE_URI[FMH::PATHTYPE_KEY::TAGS_PATH] + "fav"}, {FMH::MODEL_KEY::ICON, "love"}, {FMH::MODEL_KEY::LABEL, "Favorite"}, {FMH::MODEL_KEY::TYPE, "Quick"}};

#if defined Q_OS_LINUX && !defined Q_OS_ANDROID
        res << FMH::MODEL {{FMH::MODEL_KEY::PATH, "recentdocuments:///"}, {FMH::MODEL_KEY::ICON, "view-media-recent"}, {FMH::MODEL_KEY::LABEL, "Recent"}, {FMH::MODEL_KEY::TYPE, "Quick"}};
#endif

        return res;
    }

    if (type == FMH::PATHTYPE_KEY::PLACES_PATH) {
        res << FMStatic::getDefaultPaths();
    }

    const auto group = model.groupIndexes(static_cast<KFilePlacesModel::GroupType>(type));
    res << std::accumulate(group.constBegin(), group.constEnd(), FMH::MODEL_LIST(), [&model, &type](FMH::MODEL_LIST &list, const QModelIndex &index) -> FMH::MODEL_LIST {
        const QUrl url = model.url(index);
        if (type == FMH::PATHTYPE_KEY::PLACES_PATH && FMH::defaultPaths.contains(url.toString()))
            return list;

        if (type == FMH::PATHTYPE_KEY::PLACES_PATH && url.isLocalFile() && !FMH::fileExists(url))
            return list;

        list << FMH::MODEL {{FMH::MODEL_KEY::PATH, url.toString()},
                            {FMH::MODEL_KEY::URL, url.toString()},
                            {FMH::MODEL_KEY::ICON, model.icon(index).name()},
                            {FMH::MODEL_KEY::LABEL, model.text(index)},
                            {FMH::MODEL_KEY::NAME, model.text(index)},
                            {FMH::MODEL_KEY::TYPE, type == FMH::PATHTYPE_KEY::PLACES_PATH ? FMH::PATHTYPE_LABEL[FMH::PATHTYPE_KEY::BOOKMARKS_PATH] : FMH::PATHTYPE_LABEL[type]}};

        return list;
    });

    return res;
}

void PlacesList::setList()
{
    if (this->groups.isEmpty())
        return;

    emit this->preListChanged();

    this->list.clear();

    for (const auto &group : qAsConst(this->groups)) {
        switch (group) {
        case FMH::PATHTYPE_KEY::PLACES_PATH:
            this->list << getGroup(*this->model, FMH::PATHTYPE_KEY::PLACES_PATH);
            break;

        case FMH::PATHTYPE_KEY::QUICK_PATH:
            this->list << getGroup(*this->model, FMH::PATHTYPE_KEY::QUICK_PATH);
            break;

        case FMH::PATHTYPE_KEY::APPS_PATH:
            this->list << FM::getAppsPath();
            break;

        case FMH::PATHTYPE_KEY::DRIVES_PATH:
            this->list << getGroup(*this->model, FMH::PATHTYPE_KEY::DRIVES_PATH);
            break;

        case FMH::PATHTYPE_KEY::REMOTE_PATH:
            this->list << getGroup(*this->model, FMH::PATHTYPE_KEY::REMOTE_PATH);
            break;

        case FMH::PATHTYPE_KEY::REMOVABLE_PATH:
            this->list << getGroup(*this->model, FMH::PATHTYPE_KEY::REMOVABLE_PATH);
            break;
        }
    }

    this->setCount();
    emit this->postListChanged();
}

void PlacesList::setCount()
{
    this->watcher->removePaths(this->watcher->directories());
    for (auto &data : this->list) {
        const auto path = data[FMH::MODEL_KEY::URL];
        if (FMStatic::isDir(path)) {
            data.insert(FMH::MODEL_KEY::COUNT, "0");
            QDir dir(QUrl(path).toLocalFile());
            const auto count = dir.count();
            this->count.insert(path, count);
            this->watchPath(path);
        }
    }
}

QList<int> PlacesList::getGroups() const
{
    return this->groups;
}

void PlacesList::setGroups(const QList<int> &value)
{
    if (this->groups == value)
        return;

    this->groups = value;
    emit this->groupsChanged();
}

QVariantMap PlacesList::get(const int &index) const
{
    if (index >= this->list.size() || index < 0)
        return QVariantMap();

    const auto model = this->list.at(index);
    return FMH::toMap(model);
}

void PlacesList::clearBadgeCount(const int &index)
{
    this->list[index][FMH::MODEL_KEY::COUNT] = "0";
    emit this->updateModel(index, {FMH::MODEL_KEY::COUNT});
}

void PlacesList::removePlace(const int &index)
{
    if (index >= this->list.size() || index < 0)
        return;

    emit this->preItemRemoved(index);
    this->model->removePlace(this->model->closestItem(this->list.at(index)[FMH::MODEL_KEY::PATH]));
    this->list.removeAt(index);
    emit this->postItemRemoved();
}

bool PlacesList::contains(const QUrl &path)
{
    return this->exists(FMH::MODEL_KEY::PATH, path.toString());
}
