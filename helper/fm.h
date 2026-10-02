// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FM_H
#define FM_H

#include <QHash>
#include <QObject>
#include <QVariantMap>

class QQuickWindow;

class Fm : public QObject
{
    Q_OBJECT

public:
    explicit Fm(QObject *parent = nullptr);

    Q_INVOKABLE QString rootPath();

    // A folder window of this engine's application, with `properties` as its
    // initial properties, shown at once.
    Q_INVOKABLE QQuickWindow *openWindow(const QVariantMap &properties);
    Q_INVOKABLE static void emptyTrash();
    static bool isFixedFolder(const QUrl &folderUrl);

    // Translated names of the home and XDG user directories, keyed by path. The
    // directories keep their names on disk; only their display follows the language.
    static const QHash<QString, QString> &standardFolderNames();
    static QString standardFolderName(const QString &path);
};

#endif // FM_H
