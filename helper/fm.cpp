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

#include "fm.h"
#include "helper/iojobui.h"
#include "operations/fileoperationsclient.h"
#include "windowmanager.h"
#include <QDir>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>
#include <fileitem.h>

Fm::Fm(QObject *parent)
    : QObject(parent)
{
    CutefishFM::installIoJobUi();
}

QString Fm::rootPath()
{
    return QDir::rootPath();
}

QQuickWindow *Fm::openWindow(const QVariantMap &properties)
{
    WindowManager *manager = WindowManager::forEngine(qmlEngine(this));
    return manager ? manager->create(QUrl(QStringLiteral("qrc:/qml/main.qml")), properties) : nullptr;
}

void Fm::emptyTrash()
{
    FileOperationsClient::instance()->emptyTrash();
}

bool Fm::isFixedFolder(const QUrl &folderUrl)
{
    return Cutefish::IO::isStandardFolder(folderUrl);
}
