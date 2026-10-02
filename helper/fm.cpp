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

const QHash<QString, QString> &Fm::standardFolderNames()
{
    static const QHash<QString, QString> names = [] {
        const QList<QPair<QStandardPaths::StandardLocation, QString>> folders = {
                {QStandardPaths::HomeLocation, tr("Home")},
                {QStandardPaths::DesktopLocation, tr("Desktop")},
                {QStandardPaths::DocumentsLocation, tr("Documents")},
                {QStandardPaths::DownloadLocation, tr("Downloads")},
                {QStandardPaths::MusicLocation, tr("Music")},
                {QStandardPaths::PicturesLocation, tr("Pictures")},
                {QStandardPaths::MoviesLocation, tr("Videos")},
                {QStandardPaths::TemplatesLocation, tr("Templates")},
                {QStandardPaths::PublicShareLocation, tr("Public")},
        };

        QHash<QString, QString> result;
        for (const auto &[location, name] : folders) {
            const QString path = QStandardPaths::writableLocation(location);
            // Disabled XDG directories resolve to home, which keeps its own name.
            if (!path.isEmpty() && !result.contains(QDir::cleanPath(path)))
                result.insert(QDir::cleanPath(path), name);
        }
        return result;
    }();

    return names;
}

QString Fm::standardFolderName(const QString &path)
{
    return path.isEmpty() ? QString() : standardFolderNames().value(QDir::cleanPath(path));
}
