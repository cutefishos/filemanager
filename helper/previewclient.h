#pragma once

#include "model/foldermodel.h"

#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

class PreviewClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool previewVisible READ previewVisible NOTIFY previewVisibleChanged)
    // Whether an empty selection previews the folder. Without it, the preview
    // keeps its item until something here is selected.
    Q_PROPERTY(bool followsEmptySelection MEMBER m_followsEmptySelection)

public:
    explicit PreviewClient(QObject *parent = nullptr);
    ~PreviewClient() override;
    bool previewVisible() const;
    Q_INVOKABLE void attach(FolderModel *model, QQuickItem *view);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void activate();

signals:
    void previewVisibleChanged();

private slots:
    void visibilityChanged(bool visible);
    void navigate(const QString &source, int key, int modifiers);
    void restoreFocus(const QString &source);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    QString snapshot() const;
    bool follows() const;
    void send(const QString &method, const QVariantList &arguments, bool autoStart = false);
    void bindWindow();

    QString m_id;
    QPointer<FolderModel> m_model;
    QPointer<QQuickItem> m_view;
    QPointer<QQuickWindow> m_window;
    QTimer m_updateTimer;
    bool m_previewVisible = false;
    bool m_navigating = false;
    bool m_followsEmptySelection = true;
};
