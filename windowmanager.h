#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>

class QQmlComponent;
class QQmlEngine;
class QQuickWindow;
class QTimer;
class QWindow;
class IncubationController;

/**
 * Owns the top-level QML windows created in one QQmlEngine.
 *
 * Every window shares the engine and a per-URL component cache, so a second
 * window only instantiates objects. Closed windows are destroyed at a safe
 * point: never inside the close handler that is still running QML, never while
 * a nested event loop (QDrag::exec) above the one that created them is
 * running, and only after deleteLater()'d items inside them have gone.
 *
 * Everything the manager created must be gone before its engine is destroyed:
 * destroy the manager first, or rely on ensure()'s aboutToQuit teardown.
 */
class WindowManager : public QObject
{
    Q_OBJECT

public:
    enum CreateFlag {
        NoFlags = 0x0,
        // Shows, raises and activates the window on the next event loop turn,
        // after QML has finished its initial layout.
        ShowDeferred = 0x1,
        // Closes the window when its owner is closed or destroyed.
        CloseWithOwner = 0x2,
    };
    Q_DECLARE_FLAGS(CreateFlags, CreateFlag)

    explicit WindowManager(QQmlEngine *engine, QObject *parent = nullptr);
    ~WindowManager() override;

    static WindowManager *forEngine(const QQmlEngine *engine);

    /**
     * Returns the manager of @p engine, creating one owned by the engine for
     * hosts that only lend their engine. Its windows are destroyed on
     * QCoreApplication::aboutToQuit, so the engine must live until then.
     */
    static WindowManager *ensure(QQmlEngine *engine);

    /** The manager used for windows no QML object asked for (I/O progress). */
    static WindowManager *defaultManager();
    static void setDefault(WindowManager *manager);
    static void setDefaultIfUnset(WindowManager *manager);

    QQmlEngine *engine() const;

    /**
     * Instantiates the window at @p url with @p properties as its initial (and
     * required) properties. @p owner becomes the transient parent before the
     * window can be shown. Returns nullptr and logs why if the QML has errors
     * or its root is not a window.
     */
    QQuickWindow *create(const QUrl &url, const QVariantMap &properties = {},
                         QWindow *owner = nullptr, CreateFlags flags = NoFlags);

    /** Destroys @p window at the next safe point. Calling it again is harmless. */
    void release(QQuickWindow *window);

    /** Destroys every window now, without closing them, and drops the components. */
    void destroyAll();

    /**
     * Drives asynchronous Loaders and incubators of this engine. A plain
     * QQmlEngine has no incubation controller, which makes them synchronous.
     */
    void installIncubationController();

    int windowCount() const;
    QList<QQuickWindow *> windows() const;

signals:
    /** Emitted after a window was created or has actually been destroyed. */
    void windowCountChanged(int count);

private:
    struct Entry {
        // Identity survives into destroyed(), where the QPointer is already null.
        QQuickWindow *key = nullptr;
        QPointer<QQuickWindow> window;
        QPointer<QWindow> owner;
        CreateFlags flags;
        int loopLevel = 0;
        bool releasing = false;
    };

    QQmlComponent *component(const QUrl &url);
    Entry *entry(const QQuickWindow *window);
    void closed(QQuickWindow *window);
    void reap();
    void forget(QQuickWindow *window);

    QPointer<QQmlEngine> m_engine;
    QHash<QUrl, QQmlComponent *> m_components;
    QList<Entry> m_entries;
    QList<QPointer<QQuickWindow>> m_reaping;
    QTimer *m_reapTimer;
    IncubationController *m_incubationController = nullptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WindowManager::CreateFlags)
