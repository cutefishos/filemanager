// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILELAUNCHER_H
#define FILELAUNCHER_H

#include <QObject>

class FileLauncher : public QObject
{
    Q_OBJECT

public:
    static FileLauncher *self();
    explicit FileLauncher(QObject *parent = nullptr);

    Q_INVOKABLE bool launchApp(const QString &desktopFile, const QString &fileName);
    Q_INVOKABLE bool launchExecutable(const QString &fileName);
};

#endif // FILELAUNCHER_H
