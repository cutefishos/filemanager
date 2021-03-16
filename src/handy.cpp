/*
 *   Copyright 2018 Camilo Higuita <milo.h@aol.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "handy.h"
#include "fmh.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QIcon>
#include <QMimeData>
#include <QOperatingSystemVersion>
#include <QDBusInterface>

Handy::Handy(QObject *parent)
    : QObject(parent)
{

}

QVariantMap Handy::userInfo()
{
    QString name = qgetenv("USER");
    if (name.isEmpty())
        name = qgetenv("USERNAME");

    return QVariantMap({{FMH::MODEL_NAME[FMH::MODEL_KEY::NAME], name}});
}

QString Handy::getClipboardText()
{
    auto clipbopard = QApplication::clipboard();

    auto mime = clipbopard->mimeData();
    if (mime->hasText())
        return clipbopard->text();

    return QString();
}

QVariantMap Handy::getClipboard()
{
    QVariantMap res;

    auto clipboard = QApplication::clipboard();

    auto mime = clipboard->mimeData();
    if (mime->hasUrls())
        res.insert("urls", QUrl::toStringList(mime->urls()));

    if (mime->hasText())
        res.insert("text", mime->text());

    const QByteArray a = mime->data(QStringLiteral("application/x-kde-cutselection"));
    res.insert("cut", (!a.isEmpty() && a.at(0) == '1'));
    return res;
}

bool Handy::copyToClipboard(const QVariantMap &value, const bool &cut)
{
    auto clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();

    if (value.contains("urls"))
        mimeData->setUrls(QUrl::fromStringList(value["urls"].toStringList()));

    if (value.contains("text"))
        mimeData->setText(value["text"].toString());

    mimeData->setData(QStringLiteral("application/x-kde-cutselection"), cut ? "1" : "0");
    clipboard->setMimeData(mimeData);

    return true;
}

void Handy::setAsWallpaper(const QUrl &url)
{
    if (!url.isLocalFile())
        return;

    QDBusInterface iface("org.cutefish.Settings", "/Theme",
                         "org.cutefish.Theme",
                         QDBusConnection::sessionBus(), nullptr);
    if (iface.isValid())
        iface.call("setWallpaper", url.toLocalFile());
}

bool Handy::copyTextToClipboard(const QString &text)
{
    QApplication::clipboard()->setText(text);
    return true;
}

int Handy::version()
{
    return QOperatingSystemVersion::current().majorVersion();
}
