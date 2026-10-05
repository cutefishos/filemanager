#include "windowmanager.h"
#include "qmltypes.h"

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlIncubationController>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>

static QHash<const QQmlEngine *, WindowManager *> s_managers;
static QPointer<WindowManager> s_default;

// A deferred teardown waits this long before looking again while a nested
// event loop is still running.
static constexpr int nestedLoopRetryMs = 100;

class IncubationController : public QObject, public QQmlIncubationController
{
public:
    explicit IncubationController(QObject *parent)
        : QObject(parent)
    {
        m_timer.setInterval(0);
        QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this] {
            // Short slices keep input and painting responsive in between.
            incubateFor(5);
        });
    }

protected:
    void incubatingObjectCountChanged(int count) override
    {
        if (count > 0)
            m_timer.start();
        else
            m_timer.stop();
    }

private:
    QTimer m_timer;
};

WindowManager::WindowManager(QQmlEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_reapTimer(new QTimer(this))
{
    Q_ASSERT(engine);
    Q_ASSERT(!s_managers.contains(engine));
    s_managers.insert(engine, this);

    CutefishFM::registerImageProviders(engine);

    m_reapTimer->setSingleShot(true);
    connect(m_reapTimer, &QTimer::timeout, this, &WindowManager::reap);
}

WindowManager::~WindowManager()
{
    destroyAll();

    if (m_engine && m_engine->incubationController() == m_incubationController)
        m_engine->setIncubationController(nullptr);

    // A manager owned by its engine is deleted after the engine has gone, so
    // the key may already dangle; drop the registration by value.
    for (auto it = s_managers.begin(); it != s_managers.end();) {
        if (it.value() == this)
            it = s_managers.erase(it);
        else
            ++it;
    }
}

WindowManager *WindowManager::forEngine(const QQmlEngine *engine)
{
    return s_managers.value(engine);
}

WindowManager *WindowManager::ensure(QQmlEngine *engine)
{
    if (WindowManager *manager = forEngine(engine))
        return manager;

    auto *manager = new WindowManager(engine, engine);
    connect(qApp, &QCoreApplication::aboutToQuit, manager, &WindowManager::destroyAll);
    return manager;
}

WindowManager *WindowManager::defaultManager()
{
    return s_default;
}

void WindowManager::setDefault(WindowManager *manager)
{
    s_default = manager;
}

void WindowManager::setDefaultIfUnset(WindowManager *manager)
{
    if (!s_default)
        s_default = manager;
}

QQmlEngine *WindowManager::engine() const
{
    return m_engine;
}

QQmlComponent *WindowManager::component(const QUrl &url)
{
    if (QQmlComponent *cached = m_components.value(url))
        return cached;

    // Resources load and compile synchronously, so the component is final here.
    auto *component = new QQmlComponent(m_engine, url, QQmlComponent::PreferSynchronous, this);
    if (!component->isReady()) {
        qWarning().noquote() << "WindowManager: cannot load" << url.toString()
                             << (component->isLoading() ? QStringLiteral("(still loading)")
                                                        : component->errorString());
        delete component;
        return nullptr;
    }

    m_components.insert(url, component);
    return component;
}

bool WindowManager::preload(const QUrl &url)
{
    return component(url) != nullptr;
}

QQuickWindow *WindowManager::create(const QUrl &url, const QVariantMap &properties,
                                    QWindow *owner, CreateFlags flags)
{
    if (!m_engine)
        return nullptr;

    QQmlComponent *component = this->component(url);
    if (!component)
        return nullptr;

    QVariantMap initialProperties = properties;
    // As an initial property the transient parent is in place before a window
    // declared visible maps for the first time.
    if (owner)
        initialProperties.insert(QStringLiteral("transientParent"), QVariant::fromValue(owner));

    QObject *object = component->createWithInitialProperties(initialProperties,
                                                             m_engine->rootContext());
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        if (object) {
            qWarning().noquote() << "WindowManager:" << url.toString() << "is not a window";
            delete object;
        } else {
            qWarning().noquote() << "WindowManager: cannot create" << url.toString()
                                 << component->errorString();
        }
        return nullptr;
    }

    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);

    Entry record;
    record.key = window;
    record.window = window;
    record.owner = owner;
    record.flags = flags;
    // Windows created before the application's event loop runs belong to it.
    record.loopLevel = qMax(1, int(QThread::currentThread()->loopLevel()));
    m_entries.append(record);

    connect(window, &QObject::destroyed, this, [this, window] { forget(window); });
    // Connected after the QML handlers, which therefore run first and may reject.
    connect(window, &QQuickWindow::closing, this, [this, window] { closed(window); });

    if (owner && (flags & CloseWithOwner)) {
        connect(owner, &QObject::destroyed, window, [this, window] { release(window); });
    }

    if (flags & ShowDeferred) {
        QTimer::singleShot(0, window, [this, window] {
            const Entry *e = entry(window);
            if (!e || e->releasing)
                return;
            window->show();
            window->raise();
            window->requestActivate();
        });
    }

    emit windowCountChanged(m_entries.size());
    return window;
}

WindowManager::Entry *WindowManager::entry(const QQuickWindow *window)
{
    for (Entry &e : m_entries) {
        if (e.key == window)
            return &e;
    }
    return nullptr;
}

void WindowManager::closed(QQuickWindow *window)
{
    QPointer<QQuickWindow> guard(window);

    // The close handlers are still on the stack; decide once they returned.
    QMetaObject::invokeMethod(
            this,
            [this, guard] {
                // An accepted close destroys the platform window; a rejected
                // one (a dialog hiding while its task runs) keeps it.
                if (guard && !guard->handle())
                    release(guard);
            },
            Qt::QueuedConnection);
}

void WindowManager::release(QQuickWindow *window)
{
    Entry *e = entry(window);
    if (!e || e->releasing)
        return;

    e->releasing = true;

    QList<QPointer<QQuickWindow>> children;
    for (const Entry &child : std::as_const(m_entries)) {
        if (child.owner == window && (child.flags & CloseWithOwner) && !child.releasing
            && child.window)
            children.append(child.window);
    }

    // Closing lets a child run its own handlers (a confirmation reports it was
    // rejected); one that was never shown cannot be closed and is released.
    for (const QPointer<QQuickWindow> &child : std::as_const(children)) {
        if (!child)
            continue;
        if (child->handle() && child->isVisible())
            child->close();
        else
            release(child);
    }

    m_reaping.append(window);
    if (!m_reapTimer->isActive())
        m_reapTimer->start(0);
}

void WindowManager::reap()
{
    const int loopLevel = QThread::currentThread()->loopLevel();

    QList<QPointer<QQuickWindow>> ready;
    QList<QPointer<QQuickWindow>> waiting;
    for (const QPointer<QQuickWindow> &window : std::as_const(m_reaping)) {
        const Entry *e = window ? entry(window) : nullptr;
        if (!e)
            continue;
        // A nested loop above the one that created the window (a drag started
        // from it) may still use its items once the loop returns.
        if (loopLevel > e->loopLevel)
            waiting.append(window);
        else
            ready.append(window);
    }
    m_reaping = waiting;

    if (!ready.isEmpty()) {
        // Items deleteLater()'d inside these windows must not outlive them.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (const QPointer<QQuickWindow> &window : std::as_const(ready))
            delete window.data();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    if (!m_reaping.isEmpty())
        m_reapTimer->start(nestedLoopRetryMs);
}

void WindowManager::forget(QQuickWindow *window)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).key == window) {
            m_entries.removeAt(i);
            emit windowCountChanged(m_entries.size());
            return;
        }
    }
}

void WindowManager::destroyAll()
{
    m_reapTimer->stop();
    m_reaping.clear();

    if (!m_entries.isEmpty()) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        // Deleted without close(): a conflict dialog answers "cancel" when it
        // closes, and shutting down must not cancel a running operation.
        while (!m_entries.isEmpty()) {
            QQuickWindow *key = m_entries.constLast().key;
            if (QQuickWindow *window = m_entries.constLast().window)
                delete window;
            // Deleting normally forgets the entry; make sure the loop ends.
            if (!m_entries.isEmpty() && m_entries.constLast().key == key)
                m_entries.removeLast();
        }

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    qDeleteAll(m_components);
    m_components.clear();
}

void WindowManager::installIncubationController()
{
    if (!m_engine || m_engine->incubationController())
        return;

    m_incubationController = new IncubationController(this);
    m_engine->setIncubationController(m_incubationController);
}

int WindowManager::windowCount() const
{
    return m_entries.size();
}

QList<QQuickWindow *> WindowManager::windows() const
{
    QList<QQuickWindow *> windows;
    for (const Entry &e : m_entries) {
        if (e.window)
            windows.append(e.window);
    }
    return windows;
}
