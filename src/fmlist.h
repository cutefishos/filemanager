/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2018  Camilo Higuita <email>
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

#ifndef FMLIST_H
#define FMLIST_H

#include "fmh.h"
#include "baselist.h"
#include <QObject>

class FM;

enum STATUS_CODE : uint_fast8_t { LOADING, ERROR, READY };

/**
 * @brief The PathStatus class
 * Represents the status of a directory, be it non existance, loading or empty.
 */
class PathStatus
{
    Q_GADGET

    Q_PROPERTY(STATUS_CODE code MEMBER m_code)
    Q_PROPERTY(QString title MEMBER m_title)
    Q_PROPERTY(QString message MEMBER m_message)
    Q_PROPERTY(QString icon MEMBER m_icon)
    Q_PROPERTY(bool empty MEMBER m_empty)
    Q_PROPERTY(bool exists MEMBER m_exists)

public:
    STATUS_CODE m_code;
    QString m_title;
    QString m_message;
    QString m_icon;
    bool m_empty = false;
    bool m_exists = false;
};
Q_DECLARE_METATYPE(PathStatus)

struct NavHistory {
    void appendPath(const QUrl &path)
    {
        this->prev_history.append(path);
    }

    QUrl getPosteriorPath()
    {
        if (this->post_history.isEmpty())
            return QUrl();

        return this->post_history.takeLast();
    }

    QUrl getPreviousPath()
    {
        if (this->prev_history.isEmpty())
            return QUrl();

        if (this->prev_history.length() < 2)
            return this->prev_history.at(0);

        this->post_history.append(this->prev_history.takeLast());

        return this->prev_history.takeLast();
    }

private:
    QVector<QUrl> prev_history;
    QVector<QUrl> post_history;
};

/**
 * @brief The FMList class
 * Model for listing the file system files and directories and perfom relevant actions upon it
 */
class FMList : public BaseList
{
    Q_OBJECT

    // writable
    Q_PROPERTY(QUrl path READ getPath WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool hidden READ getHidden WRITE setHidden NOTIFY hiddenChanged)
    Q_PROPERTY(bool onlyDirs READ getOnlyDirs WRITE setOnlyDirs NOTIFY onlyDirsChanged)
    Q_PROPERTY(bool foldersFirst READ getFoldersFirst WRITE setFoldersFirst NOTIFY foldersFirstChanged)
    Q_PROPERTY(int cloudDepth READ getCloudDepth WRITE setCloudDepth NOTIFY cloudDepthChanged)

    Q_PROPERTY(QStringList filters READ getFilters WRITE setFilters NOTIFY filtersChanged)
    Q_PROPERTY(FMList::FILTER filterType READ getFilterType WRITE setFilterType NOTIFY filterTypeChanged)
    Q_PROPERTY(FMList::SORTBY sortBy READ getSortBy WRITE setSortBy NOTIFY sortByChanged)

    // readonly
    Q_PROPERTY(QString pathName READ getPathName NOTIFY pathNameChanged FINAL)
    Q_PROPERTY(FMList::PATHTYPE pathType READ getPathType NOTIFY pathTypeChanged FINAL)

    Q_PROPERTY(PathStatus status READ getStatus NOTIFY statusChanged FINAL)

    Q_PROPERTY(QUrl parentPath READ getParentPath NOTIFY pathChanged)

public:
    enum SORTBY : uint_fast8_t {
        SIZE = FMH::MODEL_KEY::SIZE,
        MODIFIED = FMH::MODEL_KEY::MODIFIED,
        DATE = FMH::MODEL_KEY::DATE,
        LABEL = FMH::MODEL_KEY::LABEL,
        MIME = FMH::MODEL_KEY::MIME,
        ADDDATE = FMH::MODEL_KEY::MIME,
        TITLE = FMH::MODEL_KEY::TITLE,
        PLACE = FMH::MODEL_KEY::PLACE,
        FORMAT = FMH::MODEL_KEY::FORMAT

    };
    Q_ENUM(SORTBY)

    enum FILTER : uint_fast8_t {
        AUDIO = FMH::FILTER_TYPE::AUDIO,
        VIDEO = FMH::FILTER_TYPE::VIDEO,
        TEXT = FMH::FILTER_TYPE::TEXT,
        IMAGE = FMH::FILTER_TYPE::IMAGE,
        DOCUMENT = FMH::FILTER_TYPE::DOCUMENT,
        COMPRESSED = FMH::FILTER_TYPE::COMPRESSED,
        FONT = FMH::FILTER_TYPE::FONT,
        NONE = FMH::FILTER_TYPE::NONE
    };
    Q_ENUM(FILTER)

    enum PATHTYPE : uint_fast8_t {
        PLACES_PATH = FMH::PATHTYPE_KEY::PLACES_PATH,
        FISH_PATH = FMH::PATHTYPE_KEY::FISH_PATH,
        MTP_PATH = FMH::PATHTYPE_KEY::MTP_PATH,
        REMOTE_PATH = FMH::PATHTYPE_KEY::REMOTE_PATH,
        DRIVES_PATH = FMH::PATHTYPE_KEY::DRIVES_PATH,
        REMOVABLE_PATH = FMH::PATHTYPE_KEY::REMOVABLE_PATH,
        TAGS_PATH = FMH::PATHTYPE_KEY::TAGS_PATH,
        APPS_PATH = FMH::PATHTYPE_KEY::APPS_PATH,
        TRASH_PATH = FMH::PATHTYPE_KEY::TRASH_PATH,
        CLOUD_PATH = FMH::PATHTYPE_KEY::CLOUD_PATH,
        QUICK_PATH = FMH::PATHTYPE_KEY::QUICK_PATH,
        OTHER_PATH = FMH::PATHTYPE_KEY::OTHER_PATH

    };
    Q_ENUM(PATHTYPE)

    enum VIEW_TYPE : uint_fast8_t {
        ICON_VIEW,
        LIST_VIEW,
        MILLERS_VIEW

    };
    Q_ENUM(VIEW_TYPE)

    Q_ENUM(STATUS_CODE)

    /**
     * @brief FMList
     * @param parent
     */
    FMList(QObject *parent = nullptr);

    /**
     * @brief items
     * @return
     */
    const FMH::MODEL_LIST &items() const final override;

    /**
     * @brief getSortBy
     * @return
     */
    FMList::SORTBY getSortBy() const;

    /**
     * @brief setSortBy
     * @param key
     */
    void setSortBy(const FMList::SORTBY &key);

    /**
     * @brief componentComplete
     */
    void componentComplete() override final;

    /**
     * @brief getPath
     * Current path being watched and model
     * @return
     * Directory URL
     */
    QUrl getPath() const;

    /**
     * @brief setPath
     * Set the directory path to be model
     * @param path
     * Directory URL
     */
    void setPath(const QUrl &path);

    /**
     * @brief getPathName
     * The short name of the current directory
     * @return
     */
    QString getPathName() const;

    /**
     * @brief getPathType
     * The type of the current path, be it LOCAl, TAGS, CLOUD, APPS, DEVICE or others
     * @return
     * Path type value
     */
    FMList::PATHTYPE getPathType() const;

    /**
     * @brief getFilters
     * The filters being applied to the current directory
     * @return
     * List of filters
     */
    QStringList getFilters() const;

    /**
     * @brief setFilters
     * FIlters to be applied as regular expressions
     * @param filters
     */
    void setFilters(const QStringList &filters);

    /**
     * @brief getFilterType
     * Filter typebeing applied, for example, filtering by AUDIO or IMAGES etc...
     * @return
     */
    FMList::FILTER getFilterType() const;

    /**
     * @brief setFilterType
     * Apply a filter type, this a quick shortcut for applying a filter on a file type such as AUDIO, IMAGE, DOCUMENT
     * @param type
     */
    void setFilterType(const FMList::FILTER &type);

    /**
     * @brief getHidden
     * Returns if the current model is including hidden files
     * @return
     */
    bool getHidden() const;

    /**
     * @brief setHidden
     * List hidden files in the model
     * @param state
     */
    void setHidden(const bool &state);

    /**
     * @brief getOnlyDirs
     * Returns if the current model is including only directories or not
     * @return
     */
    bool getOnlyDirs() const;

    /**
     * @brief setOnlyDirs
     * Only list directories when modeling a directory
     * @param state
     */
    void setOnlyDirs(const bool &state);

    /**
     * @brief getParentPath
     * Returns a URL to the parent directory of the current directory being modeled or the previous directory if the current URL is not a local file
     * @return
     */
    const QUrl getParentPath();

    /**
     * @brief getFoldersFirst
     * Returns whether directories are listed first before other files
     * @return
     */
    bool getFoldersFirst() const;

    /**
     * @brief setFoldersFirst
     * List directories first
     * @param value
     */
    void setFoldersFirst(const bool &value);

    /**
     * @brief getCloudDepth
     * @return
     */
    int getCloudDepth() const;

    /**
     * @brief setCloudDepth
     * @param value
     */
    void setCloudDepth(const int &value);

    /**
     * @brief getStatus
     * Get the current status of the current path
     * @return
     */
    PathStatus getStatus() const;

private:
    FM *fm;

    void clear();
    void reset();
    void setList();
    void assignList(const FMH::MODEL_LIST &list);
    void appendToList(const FMH::MODEL_LIST &list);
    void sortList();
    void search(const QString &query, const QUrl &path, const bool &hidden = false, const bool &onlyDirs = false, const QStringList &filters = QStringList());
    void filterContent(const QString &query, const QUrl &path);
    void setStatus(const PathStatus &status);

    FMH::MODEL_LIST list = {{}};

    QUrl path;
    QString pathName = QString();
    QStringList filters = {};

    bool onlyDirs = false;
    bool hidden = false;

    bool foldersFirst = false;
    int cloudDepth = 1;

    PathStatus m_status;

    FMList::SORTBY sort = FMList::SORTBY::MODIFIED;
    FMList::FILTER filterType = FMList::FILTER::NONE;
    FMList::PATHTYPE pathType = FMList::PATHTYPE::PLACES_PATH;

    NavHistory m_navHistory;

public slots:

    /**
     * @brief refresh
     * Refresh the model for new changes
     */
    void refresh();

    /**
     * @brief createDir
     * Create a new directory within the current directory
     * @param name
     * Name of the directory
     */
    void createDir(const QString &name);

    /**
     * @brief copyInto
     * Copy a list of file URls into the current directory
     * @param urls
     * List of files
     */
    void copyInto(const QStringList &urls);

    /**
     * @brief cutInto
     * Cut/move a list of file URLs to the current directory
     * @param urls
     * List of files
     */
    void cutInto(const QStringList &urls);

    /**
     * @brief setDirIcon
     * Changes the icon of a directory by making use of the directory config file
     * @param index
     * Index of the directory in the model
     * @param iconName
     * Name of the new icon
     */
    void setDirIcon(const int &index, const QString &iconName);

    /**
     * @brief remove
     * Remove an item from the model, this does not remove the file from the file system
     * @param index
     */
    void remove(const int &index);

    /**
     * @brief search
     * Perform a search on the current directory. The search is perfrom in another model than the current one
     * @param query
     * Query for the search
     * @param currentFMList
     * The information of the model where the search is going to be performed
     */
    void search(const QString &query, const FMList *currentFMList);

    /**
     * @brief previousPath
     * Inmediate previous path
     * @return
     */
    const QUrl previousPath();

    /**
     * @brief posteriorPath
     * Inmediate posterior path
     * @return
     */
    const QUrl posteriorPath();

signals:
    void pathChanged();
    void pathNameChanged();
    void pathTypeChanged();
    void filtersChanged();
    void filterTypeChanged();
    void hiddenChanged();
    void onlyDirsChanged();
    void sortByChanged();
    void foldersFirstChanged();
    void statusChanged();
    void cloudDepthChanged();

    void warning(QString message);
    void progress(int percent);

    void searchResultReady();
};

#endif // FMLIST_H
