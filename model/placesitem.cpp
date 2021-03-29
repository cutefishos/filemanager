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

#include "placesitem.h"
#include <QDebug>

PlacesItem::PlacesItem(const QString &displayName,
                       QUrl url,
                       QObject *parent)
    : QObject(parent)
    , m_displayName(displayName)
    , m_url(url)
{
}

QString PlacesItem::displayName() const
{
    return m_displayName;
}

void PlacesItem::setDisplayName(const QString &name)
{
    m_displayName = name;
}

QString PlacesItem::iconName() const
{
    return m_iconName;
}

void PlacesItem::setIconName(const QString &name)
{
    m_iconName = name;
}

QString PlacesItem::iconPath() const
{
    return m_iconPath;
}

void PlacesItem::setIconPath(const QString &path)
{
    m_iconPath = path;
}

QUrl PlacesItem::url() const
{
    return m_url;
}

void PlacesItem::setUrl(const QUrl &url)
{
    m_url = url;
}

QString PlacesItem::path() const
{
    return m_url.toString(QUrl::PreferLocalFile);
}

QString PlacesItem::udi() const
{
    return m_udi;
}

void PlacesItem::setUdi(const QString &udi)
{
    m_udi = udi;
}
