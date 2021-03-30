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

#include "pathbarmodel.h"
#include<QDebug>

PathBarModel::PathBarModel(QObject *parent)
    : QAbstractItemModel(parent)
{

}

PathBarModel::~PathBarModel()
{
}

QString PathBarModel::url() const
{
    return m_url;
}

void PathBarModel::setUrl(const QString &url)
{
    if (m_url != url) {
        beginResetModel();
        m_url = url;

        qDeleteAll(m_pathList);
        m_pathList.clear();

        QString _url = url;
        while (_url.endsWith("/"))
            _url.chop(1);
        _url += '/';

        int count = _url.count("/");

        for (int i = 0; i < count; ++i) {
            _url = QString(_url).left(_url.lastIndexOf("/"));
            QString label = QString(_url).right(_url.length() - _url.lastIndexOf("/") - 1);

            if (label.isEmpty())
                continue;

            PathBarItem *item = new PathBarItem;

            if (label.contains(":") && i == count - 1) {
                item->name = "/";
                item->url = QUrl(_url + "///");
            } else {
                item->name = label;
                item->url = QUrl(_url);
            }

            m_pathList.append(item);
        }

        std::reverse(m_pathList.begin(), m_pathList.end());

        endResetModel();
        emit urlChanged();
    }
}

QHash<int, QByteArray> PathBarModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[PathBarModel::NameRole] = "name";
    roleNames[PathBarModel::UrlRole] = "url";
    roleNames[PathBarModel::PathRole] = "path";
    return roleNames;
}

int PathBarModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_pathList.size();
}

int PathBarModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant PathBarModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    PathBarItem *item = m_pathList.at(index.row());

    switch (role) {
    case PathBarModel::NameRole:
        return item->name;
    case PathBarModel::UrlRole:
        return item->url;
    case PathBarModel::PathRole:
        return item->url.toString(QUrl::PreferLocalFile);
    }

    return QVariant();
}

QModelIndex PathBarModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0 || row >= m_pathList.size()) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        return QModelIndex();
    }

    return createIndex(row, column, m_pathList.at(row));
}

QModelIndex PathBarModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}
