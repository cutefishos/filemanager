// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOPICONPROVIDER_H
#define DESKTOPICONPROVIDER_H

#include <QtQuick/QQuickImageProvider>

class DesktopIconProvider : public QQuickImageProvider
{
public:
    DesktopIconProvider();

    QPixmap requestPixmap(const QString &id, QSize *realSize, const QSize &requestedSize);
};

#endif // DESKTOPICONPROVIDER_H
