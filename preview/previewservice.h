// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "helper/previewcontroller.h"

#include <QDBusContext>
#include <QQmlEngine>

class PreviewService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.FilePreview")

public:
    // The window is created in @p engine on the first Toggle and deleted with
    // this service, which therefore has to go before the engine.
    explicit PreviewService(QQmlEngine *engine, QObject *parent = nullptr);
    ~PreviewService() override;
    bool visible() const { return m_controller.visible(); }

public slots:
    void Toggle(const QString &source, const QString &snapshot);
    void Activate(const QString &source, const QString &snapshot);
    void Update(const QString &source, const QString &snapshot);
    void Release(const QString &source);
    void Key(const QString &source, int key, int modifiers);
    bool IsVisible() const;

signals:
    void VisibilityChanged(bool visible);
    void Navigate(const QString &source, int key, int modifiers);
    void RestoreFocus(const QString &source);

private:
    bool ensureWindow();
    void apply(const QString &source, const QString &snapshot, bool activate);
    QString caller() const;
    bool owns(const QString &source) const;

    PreviewController m_controller;
    QPointer<QQmlEngine> m_engine;
    QPointer<QQuickWindow> m_window;
    QString m_source;
    QString m_owner;
};
