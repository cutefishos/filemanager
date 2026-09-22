#pragma once

#include "helper/previewcontroller.h"

#include <QDBusContext>
#include <QQmlEngine>

class PreviewService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.FilePreview")

public:
    explicit PreviewService(QObject *parent = nullptr);
    ~PreviewService() override;
    PreviewController *controller() { return &m_controller; }
    QQuickWindow *window() { return m_window; }
    bool ready() const { return m_window; }

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
    void apply(const QString &source, const QString &snapshot, bool activate);
    QString caller() const;
    bool owns(const QString &source) const;

    PreviewController m_controller;
    QQmlEngine m_engine;
    QPointer<QQuickWindow> m_window;
    QString m_source;
    QString m_owner;
};
