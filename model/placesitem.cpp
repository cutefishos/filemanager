// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "placesitem.h"
#include <QDebug>

PlacesItem::PlacesItem(const QString &displayName, QUrl url, QObject *parent)
    : QObject(parent)
    , m_displayName(displayName)
    , m_url(url)
    , m_category("")
    , m_isOpticalDisc(false)
    , m_isAccessible(false)
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
    if (m_url.isValid())
        return m_url.toString(QUrl::PreferLocalFile);

    return QString();
}

QString PlacesItem::udi() const
{
    return m_udi;
}

void PlacesItem::setVolume(const Cutefish::IO::Volume &volume)
{
    m_udi = volume.id;
    m_displayName = volume.name;
    m_iconName = volume.icon;
    m_isOpticalDisc = volume.optical;
    m_canEject = volume.ejectable || volume.canPowerOff;
    m_hasFilesystem = volume.filesystem;
    m_isAccessible = !volume.mountPoints.isEmpty();
    m_url = m_isAccessible ? QUrl::fromLocalFile(volume.mountPoints.first()) : QUrl();
    emit itemChanged(this);
}

bool PlacesItem::isDevice()
{
    return !m_udi.isEmpty();
}

bool PlacesItem::setupNeeded()
{
    return m_hasFilesystem && !m_isAccessible;
}

QString PlacesItem::category() const
{
    return m_category;
}

void PlacesItem::setCategory(const QString &category)
{
    m_category = category;
}

bool PlacesItem::isOpticalDisc() const
{
    return m_isOpticalDisc;
}
