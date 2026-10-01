// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLACESITEM_H
#define PLACESITEM_H

#include <QObject>
#include <QPointer>
#include <QUrl>

#include <volumemanager.h>

class PlacesItem : public QObject
{
    Q_OBJECT

public:
    explicit PlacesItem(const QString &displayName = QString(), QUrl url = QUrl(),
                        QObject *parent = nullptr);

    QString displayName() const;
    void setDisplayName(const QString &name);

    QString iconName() const;
    void setIconName(const QString &name);

    QUrl url() const;
    void setUrl(const QUrl &url);

    QString path() const;

    QString udi() const;
    void setVolume(const Cutefish::IO::Volume &volume);

    bool isDevice();
    bool setupNeeded();

    QString category() const;
    void setCategory(const QString &category);

    bool isOpticalDisc() const;

    bool canEject() const { return m_canEject; }

signals:
    void itemChanged(PlacesItem *);

private:
    QString m_displayName;
    QString m_iconName;
    QString m_udi;
    QUrl m_url;
    QString m_category;

    bool m_isOpticalDisc;
    bool m_isAccessible;

    bool m_hasFilesystem = false;
    bool m_canEject = false;
};

#endif // PLACESITEM_H
