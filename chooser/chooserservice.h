// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QList>
#include <QObject>
#include <QPointer>

class FileChooser;
class QDBusServiceWatcher;
class QQuickWindow;
class WindowManager;

/**
 * Serves xdg-desktop-portal-cutefish's file dialogs from the session daemon.
 * Open takes the request as a JSON object and answers with the result.
 */
class FileChooserService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.FileChooser")

public:
    explicit FileChooserService(WindowManager *windows, QObject *parent = nullptr);

public slots:
    /** Replies with the dialog's result once it is answered or closed. */
    QString Open(const QString &id, const QString &request);

    /** Cancels the caller's dialog @p id; Open then replies with a cancellation. */
    void Close(const QString &id);

private:
    struct Dialog {
        QString caller;
        QString id;
        QPointer<FileChooser> chooser;
        QPointer<QQuickWindow> window;
    };

    Dialog *find(const QString &caller, const QString &id);
    void finished(FileChooser *chooser);

    QPointer<WindowManager> m_windows;
    QDBusServiceWatcher *m_watcher;
    QList<Dialog> m_dialogs;
};
