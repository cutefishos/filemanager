// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QUrl>

class OperationTask;
class PreviewService;
class QQmlEngine;
class QQuickWindow;
class QTimer;
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

    /** Returns the daemon's freed heap to the system once it has stayed idle. */
    void scheduleHeapTrim();

private:
    QQmlEngine *m_engine = nullptr;
    WindowManager *m_windows = nullptr;
    PreviewService *m_preview = nullptr;
    QTimer *m_heapTrimTimer = nullptr;
    bool m_servicesRegistered = false;
    bool m_dbusActivated = false;
    bool m_daemon = false;
    bool m_idleCheckScheduled = false;
    int m_exitCode = EXIT_SUCCESS;
};

#endif // APPLICATION_H
