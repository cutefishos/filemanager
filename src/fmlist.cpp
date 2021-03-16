/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2018  camilo higuita <milo.h@aol.com>
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

#include "fmlist.h"
#include "fm.h"

#if defined Q_OS_LINUX && !defined Q_OS_ANDROID
#include <KIO/EmptyTrashJob>
#endif

#include <QFuture>
#include <QObject>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <QtConcurrent>

FMList::FMList(QObject *parent)
    : BaseList(parent)
    , fm(new FM(this))
{
    qRegisterMetaType<FMList *>("const FMList*"); // this is needed for QML to know of FMList in the search method
    connect(this->fm, &FM::cloudServerContentReady, [&](const FMH::MODEL_LIST &list, const QUrl &url) {
        if (this->path == url) {
            this->assignList(list);
        }
    });

    connect(this->fm, &FM::pathContentReady, [&](QUrl) {
        emit this->preListChanged();
        this->sortList();
        this->setStatus({STATUS_CODE::READY, this->list.isEmpty() ? "Nothing here!" : "", this->list.isEmpty() ? "This place seems to be empty" : "", this->list.isEmpty() ? "folder-add" : "", this->list.isEmpty(), true});
        emit this->postListChanged();
    });

    connect(this->fm, &FM::pathContentItemsChanged, [&](QVector<QPair<FMH::MODEL, FMH::MODEL>> res) {
        for (const auto &item : qAsConst(res)) {
            const auto index = this->indexOf(FMH::MODEL_KEY::PATH, item.first[FMH::MODEL_KEY::PATH]);

            if (index >= this->list.size() || index < 0)
                return;

            this->list[index] = item.second;
            emit this->updateModel(index, FMH::modelRoles(item.second));
        }
    });

    connect(this->fm, &FM::pathContentItemsReady, [&](FMH::PATH_CONTENT res) {
        if (res.path != this->path)
            return;

        this->appendToList(res.content);
    });

    connect(this->fm, &FM::pathContentItemsRemoved, [&](FMH::PATH_CONTENT res) {
        if (res.path != this->path)
            return;

        if (!FMH::fileExists(res.path)) {
            this->setStatus({STATUS_CODE::ERROR, "Error", "This URL cannot be listed", "documentinfo", true, false});
            return;
        }

        for (const auto &item : qAsConst(res.content)) {
            const auto index = this->indexOf(FMH::MODEL_KEY::PATH, item[FMH::MODEL_KEY::PATH]);
            qDebug() << "SUPOSSED TO REMOVED THIS FORM THE LIST" << index << this->list.count() << item[FMH::MODEL_KEY::PATH];

            this->remove(index);
        }

        this->setStatus({STATUS_CODE::READY, this->list.isEmpty() ? "Nothing here!" : "", this->list.isEmpty() ? "This place seems to be empty" : "", this->list.isEmpty() ? "folder-add" : "", this->list.isEmpty(), true});
    });

    connect(this->fm, &FM::warningMessage, [&](const QString &message) {
        emit this->warning(message);
    });

    connect(this->fm, &FM::loadProgress, [&](const int &percent) {
        emit this->progress(percent);
    });

    connect(this->fm, &FM::pathContentChanged, [&](const QUrl &path) {
        qDebug() << "FOLDER PATH CHANGED" << path;
        if (path != this->path)
            return;
        this->sortList();
    });

    connect(this->fm, &FM::newItem, [&](const FMH::MODEL &item, const QUrl &url) {
        if (this->path == url) {
            emit this->preItemAppended();
            this->list << item;
            emit this->postItemAppended();
        }
    });
}

void FMList::assignList(const FMH::MODEL_LIST &list)
{
    emit this->preListChanged();
    this->list = list;
    this->sortList();
    this->setStatus({STATUS_CODE::READY, this->list.isEmpty() ? "Nothing here!" : "", this->list.isEmpty() ? "This place seems to be empty" : "", this->list.isEmpty() ? "folder-add" : "", this->list.isEmpty(), true});
    emit this->postListChanged();
}

void FMList::appendToList(const FMH::MODEL_LIST &list)
{
    emit this->preItemsAppended(list.size());
    this->list << list;
    emit this->postItemAppended();
}

void FMList::clear()
{
    emit this->preListChanged();
    this->list.clear();
    emit this->postListChanged();
}

void FMList::setList()
{
    qDebug() << "PATHTYPE FOR URL" << pathType << this->path.toString() << this->filters << this;
    this->clear();

    switch (this->pathType) {
    case FMList::PATHTYPE::CLOUD_PATH:
        this->fm->getCloudServerContent(this->path.toString(), this->filters, this->cloudDepth);
        break; // ASYNC

    default: {
        const bool exists = this->path.isLocalFile() ? FMH::fileExists(this->path) : true;
        if (!exists)
            this->setStatus({STATUS_CODE::ERROR, "Error", "This URL cannot be listed", "documentinfo", this->list.isEmpty(), exists});
        else {
            this->fm->getPathContent(this->path, this->hidden, this->onlyDirs, QStringList() << this->filters << FMH::FILTER_LIST[static_cast<FMH::FILTER_TYPE>(this->filterType)]);
        }
        break; // ASYNC
    }
    }
}

void FMList::reset()
{
    this->setList();
}

const FMH::MODEL_LIST &FMList::items() const
{
    return this->list;
}

FMList::SORTBY FMList::getSortBy() const
{
    return this->sort;
}

void FMList::setSortBy(const FMList::SORTBY &key)
{
    if (this->sort == key)
        return;

    emit this->preListChanged();

    this->sort = key;
    this->sortList();

    emit this->sortByChanged();
    emit this->postListChanged();
}

void FMList::sortList()
{
    const FMH::MODEL_KEY key = static_cast<FMH::MODEL_KEY>(this->sort);
    auto index = 0;

    if (this->foldersFirst) {
        qSort(this->list.begin(), this->list.end(), [](const FMH::MODEL &e1, const FMH::MODEL &e2) -> bool {
            Q_UNUSED(e2)
            const auto key = FMH::MODEL_KEY::MIME;
            return e1[key] == "inode/directory";
        });

        for (const auto &item : qAsConst(this->list))
            if (item[FMH::MODEL_KEY::MIME] == "inode/directory")
                index++;
            else
                break;

        std::sort(this->list.begin(), this->list.begin() + index, [&key](const FMH::MODEL &e1, const FMH::MODEL &e2) -> bool {
            switch (key) {
            case FMH::MODEL_KEY::SIZE: {
                if (e1[key].toDouble() > e2[key].toDouble())
                    return true;
                break;
            }

            case FMH::MODEL_KEY::MODIFIED:
            case FMH::MODEL_KEY::DATE: {
                auto currentTime = QDateTime::currentDateTime();

                auto date1 = QDateTime::fromString(e1[key], Qt::TextDate);
                auto date2 = QDateTime::fromString(e2[key], Qt::TextDate);

                if (date1.secsTo(currentTime) < date2.secsTo(currentTime))
                    return true;

                break;
            }

            case FMH::MODEL_KEY::LABEL: {
                const auto str1 = QString(e1[key]).toLower();
                const auto str2 = QString(e2[key]).toLower();

                if (str1 < str2)
                    return true;
                break;
            }

            default:
                if (e1[key] < e2[key])
                    return true;
            }

            return false;
        });
    }

    std::sort(this->list.begin() + index, this->list.end(), [key](const FMH::MODEL &e1, const FMH::MODEL &e2) -> bool {
        switch (key) {
        case FMH::MODEL_KEY::MIME:
            if (e1[key] == "inode/directory")
                return true;
            break;

        case FMH::MODEL_KEY::SIZE: {
            if (e1[key].toDouble() > e2[key].toDouble())
                return true;
            break;
        }

        case FMH::MODEL_KEY::MODIFIED:
        case FMH::MODEL_KEY::DATE: {
            auto currentTime = QDateTime::currentDateTime();

            auto date1 = QDateTime::fromString(e1[key], Qt::TextDate);
            auto date2 = QDateTime::fromString(e2[key], Qt::TextDate);

            if (date1.secsTo(currentTime) < date2.secsTo(currentTime))
                return true;

            break;
        }

        case FMH::MODEL_KEY::LABEL: {
            const auto str1 = QString(e1[key]).toLower();
            const auto str2 = QString(e2[key]).toLower();

            if (str1 < str2)
                return true;
            break;
        }

        default:
            if (e1[key] < e2[key])
                return true;
        }

        return false;
    });
}

QString FMList::getPathName() const
{
    return this->pathName;
}

QUrl FMList::getPath() const
{
    return this->path;
}

void FMList::setPath(const QUrl &path)
{
    QUrl path_ = QUrl::fromUserInput(path.toString().trimmed());

    if (this->path == path_)
        return;

    this->path = path_;
    m_navHistory.appendPath(this->path);

    this->setStatus({STATUS_CODE::LOADING, "Loading content", "Almost ready!", "view-refresh", true, false});

    const auto __scheme = this->path.scheme();
    this->pathName = QDir(this->path.toLocalFile()).dirName();

    if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::CLOUD_PATH]) {
        this->pathType = FMList::PATHTYPE::CLOUD_PATH;

    } else if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::APPS_PATH]) {
        this->pathType = FMList::PATHTYPE::APPS_PATH;

    } else if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::TAGS_PATH]) {
        this->pathType = FMList::PATHTYPE::TAGS_PATH;
        this->pathName = this->path.path();

    } else if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::TRASH_PATH]) {
        this->pathType = FMList::PATHTYPE::TRASH_PATH;
        this->pathName = "Trash";

    } else if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::PLACES_PATH]) {
        this->pathType = FMList::PATHTYPE::PLACES_PATH;

    } else if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::MTP_PATH]) {
        this->pathType = FMList::PATHTYPE::MTP_PATH;

    } else if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::FISH_PATH]) {
        this->pathType = FMList::PATHTYPE::FISH_PATH;

    } else if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::REMOTE_PATH]) {
        this->pathType = FMList::PATHTYPE::REMOTE_PATH;

    } else if (__scheme == FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::DRIVES_PATH]) {
        this->pathType = FMList::PATHTYPE::DRIVES_PATH;
    } else {
        this->pathType = FMList::PATHTYPE::OTHER_PATH;
    }

    emit this->pathNameChanged();
    emit this->pathTypeChanged();
    emit this->pathChanged();
}

FMList::PATHTYPE FMList::getPathType() const
{
    return this->pathType;
}

QStringList FMList::getFilters() const
{
    return this->filters;
}

void FMList::setFilters(const QStringList &filters)
{
    if (this->filters == filters)
        return;

    this->filters = filters;

    emit this->filtersChanged();
}

FMList::FILTER FMList::getFilterType() const
{
    return this->filterType;
}

void FMList::setFilterType(const FMList::FILTER &type)
{
    if (this->filterType == type)
        return;

    this->filterType = type;

    emit this->filterTypeChanged();
}

bool FMList::getHidden() const
{
    return this->hidden;
}

void FMList::setHidden(const bool &state)
{
    if (this->hidden == state)
        return;

    this->hidden = state;

    emit this->hiddenChanged();
}

bool FMList::getOnlyDirs() const
{
    return this->onlyDirs;
}

void FMList::setOnlyDirs(const bool &state)
{
    if (this->onlyDirs == state)
        return;

    this->onlyDirs = state;

    emit this->onlyDirsChanged();
}

void FMList::refresh()
{
    emit this->pathChanged();
}

void FMList::createDir(const QString &name)
{
    if (this->pathType == FMList::PATHTYPE::CLOUD_PATH) {
#ifdef COMPONENT_SYNCING
        this->fm->createCloudDir(QString(this->path.toString()).replace(FMH::PATHTYPE_SCHEME[FMH::PATHTYPE_KEY::CLOUD_PATH] + "/" + this->fm->sync->getUser(), ""), name);
#endif
    } else {
        FMStatic::createDir(this->path, name);
    }
}

void FMList::copyInto(const QStringList &urls)
{
    this->fm->copy(QUrl::fromStringList(urls), this->path);
}

void FMList::cutInto(const QStringList &urls)
{
    this->fm->cut(QUrl::fromStringList(urls), this->path);
    // 	else if(this->pathType == FMList::PATHTYPE::CLOUD_PATH)
    // 	{
    // 		this->fm->createCloudDir(QString(this->path).replace(FMH::PATHTYPE_NAME[FMList::PATHTYPE::CLOUD_PATH]+"/"+this->fm->sync->getUser(), ""), name);
    // 	}
}

void FMList::setDirIcon(const int &index, const QString &iconName)
{
    if (index >= this->list.size() || index < 0)
        return;

    //    const auto index_ = this->mappedIndex(index);

    const auto path = QUrl(this->list.at(index)[FMH::MODEL_KEY::PATH]);

    if (!FMStatic::isDir(path))
        return;

    FMH::setDirConf(path.toString() + "/.directory", "Desktop Entry", "Icon", iconName);

    this->list[index][FMH::MODEL_KEY::ICON] = iconName;
    emit this->updateModel(index, QVector<int> {FMH::MODEL_KEY::ICON});
}

const QUrl FMList::getParentPath()
{
    switch (this->pathType) {
    case FMList::PATHTYPE::PLACES_PATH:
        return FMStatic::parentDir(this->path).toString();
    default:
        return this->previousPath();
    }
}

const QUrl FMList::posteriorPath()
{
    const auto url = m_navHistory.getPosteriorPath();

    if (url.isEmpty())
        return this->path;

    return url;
}

const QUrl FMList::previousPath()
{
    const auto url = m_navHistory.getPreviousPath();

    if (url.isEmpty())
        return this->path;

    return url;
}

bool FMList::getFoldersFirst() const
{
    return this->foldersFirst;
}

void FMList::setFoldersFirst(const bool &value)
{
    if (this->foldersFirst == value)
        return;

    emit this->preListChanged();

    this->foldersFirst = value;

    emit this->foldersFirstChanged();

    this->sortList();

    emit this->postListChanged();
}

void FMList::search(const QString &query, const FMList *currentFMList)
{
    this->search(query, currentFMList->getPath(), currentFMList->getHidden(), currentFMList->getOnlyDirs(), currentFMList->getFilters());
}

void FMList::componentComplete()
{
    connect(this, &FMList::pathChanged, this, &FMList::setList);
    connect(this, &FMList::filtersChanged, this, &FMList::setList);
    connect(this, &FMList::filterTypeChanged, this, &FMList::setList);
    connect(this, &FMList::hiddenChanged, this, &FMList::setList);
    connect(this, &FMList::onlyDirsChanged, this, &FMList::setList);

    this->setList();
}

void FMList::search(const QString &query, const QUrl &path, const bool &hidden, const bool &onlyDirs, const QStringList &filters)
{
    qDebug() << "SEARCHING FOR" << query << path;

    if (!path.isLocalFile()) {
        qWarning() << "URL recived is not a local file. So search will only filter the content" << path;
        this->filterContent(query, path);
        return;
    }

    QFutureWatcher<FMH::PATH_CONTENT> *watcher = new QFutureWatcher<FMH::PATH_CONTENT>;
    connect(watcher, &QFutureWatcher<FMH::MODEL_LIST>::finished, [=]() {
        const auto res = watcher->future().result();

        this->assignList(res.content);
        emit this->searchResultReady();

        watcher->deleteLater();
    });

    QFuture<FMH::PATH_CONTENT> t1 = QtConcurrent::run([=]() -> FMH::PATH_CONTENT {
        FMH::PATH_CONTENT res;
        res.path = path.toString();
        res.content = FMStatic::search(query, path, hidden, onlyDirs, filters);
        return res;
    });
    watcher->setFuture(t1);
}

void FMList::filterContent(const QString &query, const QUrl &path)
{
    if (this->list.isEmpty()) {
        qDebug() << "Can not filter content. List is empty";
        return;
    }

    QFutureWatcher<FMH::PATH_CONTENT> *watcher = new QFutureWatcher<FMH::PATH_CONTENT>;
    connect(watcher, &QFutureWatcher<FMH::MODEL_LIST>::finished, [=]() {
        const auto res = watcher->future().result();

        this->assignList(res.content);
        emit this->searchResultReady();

        watcher->deleteLater();
    });

    QFuture<FMH::PATH_CONTENT> t1 = QtConcurrent::run([=]() -> FMH::PATH_CONTENT {
        FMH::MODEL_LIST m_content;
        FMH::PATH_CONTENT res;

        for (const auto &item : qAsConst(this->list)) {
            if (item[FMH::MODEL_KEY::LABEL].contains(query, Qt::CaseInsensitive) || item[FMH::MODEL_KEY::SUFFIX].contains(query, Qt::CaseInsensitive) || item[FMH::MODEL_KEY::MIME].contains(query, Qt::CaseInsensitive)) {
                m_content << item;
            }
        }

        res.path = path.toString();
        res.content = m_content;
        return res;
    });
    watcher->setFuture(t1);
}

int FMList::getCloudDepth() const
{
    return this->cloudDepth;
}

void FMList::setCloudDepth(const int &value)
{
    if (this->cloudDepth == value)
        return;

    this->cloudDepth = value;

    emit this->cloudDepthChanged();
}

PathStatus FMList::getStatus() const
{
    return this->m_status;
}

void FMList::setStatus(const PathStatus &status)
{
    this->m_status = status;
    emit this->statusChanged();
}

void FMList::remove(const int &index)
{
    if (index >= this->list.size() || index < 0)
        return;

    emit this->preItemRemoved(index);
    this->list.remove(index);
    emit this->postItemRemoved();
}
