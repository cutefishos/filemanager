// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MIMEAPPMANAGER_H
#define MIMEAPPMANAGER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QTimer>
#include <QMap>

#include "xdgdesktopfile.h"

class QMimeType;
class MimeAppManager : public QObject
{
    Q_OBJECT

public:
    static MimeAppManager *self();
    explicit MimeAppManager(QObject *parent = nullptr);

    QStringList desktopPaths();
    QString mimeAppsListFilePath();
    void initApplications();

    QString getDefaultAppByFilePath(const QString &filePath);
    QString getDefaultAppByMimeType(const QMimeType &mimeType);
    QString getDefaultAppDesktopByMimeType(const QString &mimeType);
    Q_INVOKABLE bool setDefaultAppForType(const QString &mimeType, const QString &app);
    Q_INVOKABLE bool setDefaultAppForFile(const QString &filePath, const QString &desktop);

    QStringList getRecommendedAppsByFilePath(const QString &filePath);
    QStringList getRecommendedAppsByMimeType(const QMimeType &mimeType);

    Q_INVOKABLE QVariantList recommendedApps(const QUrl &url);

    Q_INVOKABLE void launchTerminal(const QString &path);

private slots:
    void onFileChanged(const QString &path);

private:
    QStringList m_desktopFiles;
    QMap<QString, QStringList> m_mimeApps;

    QMap<QString, XdgDesktopFile> m_videoMimeApps;
    QMap<QString, XdgDesktopFile> m_imageMimeApps;
    QMap<QString, XdgDesktopFile> m_textMimeApps;
    QMap<QString, XdgDesktopFile> m_audioMimeApps;
    QMap<QString, XdgDesktopFile> m_desktopObjects;

    QList<XdgDesktopFile> m_terminalApps;

    QFileSystemWatcher *m_fileSystemWatcher;
    QTimer *m_updateTimer;
};

#endif // MIMEAPPMANAGER_H
