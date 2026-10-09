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

    /** Runs the launcher or program at @p path itself. */
    bool launchFile(const QString &path);

    /**
     * Whether @p path may run without asking: only a launcher in an applications
     * directory. No other launcher runs, and a program always asks.
     */
    static bool isTrusted(const QString &path);

    /** Marks the program at @p path executable for its owner, as the user agreed to, then runs it. */
    Q_INVOKABLE bool trustAndLaunch(const QString &path);
};

#endif // FILELAUNCHER_H
