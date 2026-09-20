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

#include "dirlister.h"
#include "mimetype/xdgdesktopfile.h"

DirLister::DirLister(QObject *parent)
    : Cutefish::IO::DirectoryLister(parent)
{
    setItemPreparer([](Cutefish::IO::FileItem &item) {
        if (!item.isDesktopFile() || !item.isFile())
            return;
        const XdgDesktopFile desktopFile(item.localPath());
        item.setDisplayName(desktopFile.localeName());
        item.setIconName(desktopFile.value(QStringLiteral("Icon")).toString());

        if (desktopFile.value(QStringLiteral("Type")).toString() == QLatin1String("Link"))
            item.setTargetUrl(QUrl(desktopFile.value(QStringLiteral("URL")).toString()));
    });
}

DirLister::~DirLister()
{
}
