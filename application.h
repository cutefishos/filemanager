/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reion@cutefishos.com>
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

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QUrl>

class OperationTask;
class QQmlEngine;
class QQuickWindow;
class WindowManager;

class Application : public QApplication
{
    Q_OBJECT

public:
    explicit Application(int &argc, char **argv);
    ~Application() override;

    /** Handles the command line, then runs until no window or own task is left. */
    int run();

    // com.cutefish.FileManager
    void openFiles(const QStringList &paths);
    void moveToTrash(const QStringList &paths);
    void emptyTrash();

    // org.freedesktop.FileManager1
    void showItems(const QList<QUrl> &items);
    void showItemProperties(const QList<QUrl> &items);

    WindowManager *windowManager() const;

private:
    /** Takes the D-Bus names; false if another instance holds them. */
    bool registerServices();
    void unregisterServices();

    /** Serves the session from its start until it ends, with or without windows. */
    void startDaemon();
    WindowManager *ensureWindows();
    QQuickWindow *openWindow(const QString &path, const QStringList &selection = {});
    QStringList formatUriList(const QStringList &list);

    /** Starts the trash job for @p paths, or returns nullptr if there is none. */
    OperationTask *startTrashJob(const QStringList &paths);

    bool parseCommandLineArgs();

    void scheduleIdleCheck();
    void quitIfIdle();
    bool isIdle() const;

private:
    QQmlEngine *m_engine = nullptr;
    WindowManager *m_windows = nullptr;
    bool m_servicesRegistered = false;
    bool m_dbusActivated = false;
    bool m_daemon = false;
    bool m_idleCheckScheduled = false;
    int m_exitCode = EXIT_SUCCESS;
};

#endif // APPLICATION_H
