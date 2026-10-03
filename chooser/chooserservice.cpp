// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chooserservice.h"
#include "chooserwindow.h"
#include "filechooser.h"
#include "windowmanager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QQuickWindow>

FileChooserService::FileChooserService(WindowManager *windows, QObject *parent)
    : QObject(parent)
    , m_windows(windows)
    , m_watcher(new QDBusServiceWatcher(this))
{
    // Compiled now, so the first dialog opens as fast as later ones.
    preloadChooserWindow(windows);

    m_watcher->setConnection(QDBusConnection::sessionBus());
    m_watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &name) {
        // Nobody is left to read the result, so the dialog goes.
        QList<QPointer<FileChooser>> choosers;
        for (const Dialog &dialog : std::as_const(m_dialogs)) {
            if (dialog.caller == name)
                choosers.append(dialog.chooser);
        }
        for (const QPointer<FileChooser> &chooser : std::as_const(choosers)) {
            if (chooser)
                chooser->cancel();
        }
    });
}

FileChooserService::Dialog *FileChooserService::find(const QString &caller, const QString &id)
{
    for (Dialog &dialog : m_dialogs) {
        if (dialog.caller == caller && dialog.id == id)
            return &dialog;
    }
    return nullptr;
}

QString FileChooserService::Open(const QString &id, const QString &request)
{
    const QString caller = message().service();
    const auto document = QJsonDocument::fromJson(request.toUtf8());
    if (!document.isObject() || !FileChooser::isValidRequest(document.object())
        || find(caller, id)) {
        sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid file chooser request"));
        return {};
    }

    auto *chooser = new FileChooser(document.object());
    QQuickWindow *window = m_windows ? openChooserWindow(m_windows, chooser) : nullptr;
    if (!window) {
        delete chooser;
        sendErrorReply(QDBusError::Failed, QStringLiteral("Cannot create the file chooser"));
        return {};
    }
    // The window's bindings read the chooser until the window is gone.
    connect(window, &QObject::destroyed, chooser, &QObject::deleteLater);

    setDelayedReply(true);
    const QDBusMessage call = message();
    const QDBusConnection bus = connection();
    connect(chooser, &FileChooser::finished, this, [this, chooser, call, bus](const QJsonObject &result) {
        bus.send(call.createReply(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))));
        finished(chooser);
    });

    m_dialogs.append({caller, id, chooser, window});
    m_watcher->addWatchedService(caller);
    return {};
}

void FileChooserService::Close(const QString &id)
{
    if (Dialog *dialog = find(message().service(), id)) {
        if (dialog->chooser)
            dialog->chooser->cancel();
    }
}

void FileChooserService::finished(FileChooser *chooser)
{
    QString caller;
    for (int i = 0; i < m_dialogs.size(); ++i) {
        if (m_dialogs.at(i).chooser == chooser) {
            caller = m_dialogs.at(i).caller;
            if (m_windows && m_dialogs.at(i).window)
                m_windows->release(m_dialogs.at(i).window);
            m_dialogs.removeAt(i);
            break;
        }
    }

    for (const Dialog &dialog : std::as_const(m_dialogs)) {
        if (dialog.caller == caller)
            return;
    }
    m_watcher->removeWatchedService(caller);
}
