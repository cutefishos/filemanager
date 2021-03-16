#include "placesitem.h"
#include <QDebug>

PlacesItem::PlacesItem(const QString &displayName,
                       const QString &iconName,
                       QUrl url,
                       QObject *parent)
    : QObject(parent)
    , m_displayName(displayName)
    , m_iconName(iconName)
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
    return m_url.toString();
}
