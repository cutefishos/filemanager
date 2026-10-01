#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QVariantMap>
#include "cio/cfilesizejob.h"

#include "model/foldermodel.h"

class PreviewController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)
    Q_PROPERTY(QVariantMap file READ file NOTIFY fileChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY fileChanged)
    // The requesting window's device pixel ratio. A preview window that has
    // not been shown yet only knows the output's rounded-up integer scale.
    Q_PROPERTY(qreal sourceRatio READ sourceRatio NOTIFY changed)

public:
    explicit PreviewController(QObject *parent = nullptr);
    ~PreviewController() override;
    bool visible() const;
    QVariantList items() const;
    int currentIndex() const;
    QVariantMap file() const;
    bool loading() const;
    qreal sourceRatio() const { return m_sourceRatio; }

    Q_INVOKABLE void attach(FolderModel *model, QQuickItem *view, QQuickWindow *window);
    Q_INVOKABLE void toggle();
    void setWindow(QQuickWindow *window);
    void present(const QVariantList &items, int currentIndex, bool activate, qreal ratio = 0);
    static QVariantMap snapshot(FolderModel *model, QQuickItem *view);
    Q_INVOKABLE void close();
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void step(int delta);
    Q_INVOKABLE void openCurrent();

signals:
    void changed();
    void fileChanged();
    void showRequested();
    void navigateRequested(int key, int modifiers);
    void closed();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void synchronize();
    void load();
    void stopFolderSize();
    void loadFolderSize(const QUrl &url);

    QPointer<FolderModel> m_model;
    QPointer<QQuickItem> m_view;
    QPointer<QQuickWindow> m_window;
    QTimer m_selectionTimer;
    QPointer<CFileSizeJob> m_sizeJob;
    QVariantList m_items;
    QVariantMap m_file;
    int m_index = 0;
    quint64 m_generation = 0;
    bool m_visible = false;
    bool m_loading = false;
    bool m_showPending = false;
    qreal m_sourceRatio = 0;
};
