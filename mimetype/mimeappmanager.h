// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MIMEAPPMANAGER_H
#define MIMEAPPMANAGER_H

#include <QObject>
#include <QStringList>
#include <QVariantList>

class QMimeType;
class QUrl;

// Which applications open a file, from the session's ApplicationRegistry and the
// user's mimeapps.list. Applications are named by their desktop file paths.
class MimeAppManager : public QObject
{
    Q_OBJECT

public:
    static MimeAppManager *self();
    explicit MimeAppManager(QObject *parent = nullptr);

    QString getDefaultAppByFilePath(const QString &filePath);
    QString getDefaultAppByMimeType(const QMimeType &mimeType);
    Q_INVOKABLE bool setDefaultAppForType(const QString &mimeType, const QString &app);
    Q_INVOKABLE bool setDefaultAppForFile(const QString &filePath, const QString &desktop);

    QStringList getRecommendedAppsByFilePath(const QString &filePath);
    QStringList getRecommendedAppsByMimeType(const QMimeType &mimeType);

    Q_INVOKABLE QVariantList recommendedApps(const QUrl &url);

    Q_INVOKABLE void launchTerminal(const QString &path);
};

#endif // MIMEAPPMANAGER_H
