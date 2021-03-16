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

#ifndef PLACESLIST_H
#define PLACESLIST_H

#include "baselist.h"
#include <QObject>

class FM;
class KFilePlacesModel;
class QFileSystemWatcher;
class PlacesList : public BaseList
{
    Q_OBJECT
    Q_PROPERTY(QList<int> groups READ getGroups WRITE setGroups NOTIFY groupsChanged)

public:
    PlacesList(QObject *parent = nullptr);

    const FMH::MODEL_LIST &items() const override;

    QList<int> getGroups() const;
    void setGroups(const QList<int> &value);

    void componentComplete() override final;

    /**
     * @brief get
     * Gets a item in the model.
     * @param index
     * Index of the item in the model. The given index is not mapped to a filtered or sorted model
     * @return
     * The data of the place
     */
    QVariantMap get(const int &index) const;

protected:
    void setList();
    void reset();

public slots:
    /**
     * @brief clearBadgeCount
     * Clears the count associated to a place at a given index in the model
     * @param index
     */
    void clearBadgeCount(const int &index);

    /**
     * @brief removePlace
     * Removes a place from the model and if the data at the given index is a file URL bookmark then it gets removed from the bookmarks.
     * @param index
     * Index of the item to be removed in the model
     */
    void removePlace(const int &index);

    /**
     * @brief contains
     * Checks of a file URL exists in the places model
     * @param path
     * File URL to be checked
     * @return
     * True if it exists otherwise false
     */
    bool contains(const QUrl &path);

private:
    FM *fm;
    FMH::MODEL_LIST list;
    KFilePlacesModel *model;
    QHash<QString, int> count;

    QList<int> groups;

    QFileSystemWatcher *watcher;
    void watchPath(const QString &path);

    void setCount();

    static FMH::MODEL_LIST getGroup(const KFilePlacesModel &model, const FMH::PATHTYPE_KEY &type);

signals:
    void groupsChanged();
    void bookmarksChanged();
};
#endif // PLACESLIST_H
