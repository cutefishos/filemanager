// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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
