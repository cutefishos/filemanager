// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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
