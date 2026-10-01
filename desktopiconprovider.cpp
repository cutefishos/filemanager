// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopiconprovider.h"
#include <QIcon>
#include <QCommonStyle>

DesktopIconProvider::DesktopIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{

}

QPixmap DesktopIconProvider::requestPixmap(const QString &fullId, QSize *realSize,
                                           const QSize &requestedSize)
{
    // Theme.iconThemeQuery only exists to make the URL unique per theme.
    const QString id = fullId.section(QLatin1Char('?'), 0, 0);

    QSize size(requestedSize);
    if (size.width() < 1)
        size.setWidth(128);
    if (size.height() < 1)
        size.setHeight(128);

    if (realSize)
        *realSize = size;

    if (id.startsWith(QLatin1Char('/')))
        return QPixmap(id).scaled(size);

    QIcon icon = QIcon::fromTheme(id);
    if (icon.isNull()) {
        QCommonStyle fallback;
        const bool folder = id == QLatin1String("inode-directory") || id.startsWith(QLatin1String("folder"));
        icon = fallback.standardIcon(folder ? QStyle::SP_DirIcon : QStyle::SP_FileIcon);
    }

    return icon.pixmap(size);
}
