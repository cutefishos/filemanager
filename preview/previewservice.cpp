#include "previewservice.h"
#include "mimetype/mimeappmanager.h"
#include "qmltypes.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QQmlComponent>
#include <QScreen>

PreviewService::PreviewService(QObject *parent)
    : QObject(parent)
{
    // Scans the desktop entries in the background, for the "Open with" button.
    MimeAppManager::self();
    CutefishFM::registerImageProviders(&m_engine);
    QQmlComponent component(&m_engine, QUrl("qrc:/qml/PreviewWindow.qml"));
    m_window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties(
            {{"controller", QVariant::fromValue(&m_controller)}}));
    if (!m_window) {
        qWarning() << component.errors();
        return;
    }
    m_window->QObject::setParent(&m_engine);
    m_window->setFlags(m_window->flags() | Qt::WindowStaysOnTopHint);
    m_controller.setWindow(m_window);
    connect(&m_controller, &PreviewController::closed, this, [this] {
        emit VisibilityChanged(false);
        emit RestoreFocus(m_source);
    });
    connect(&m_controller, &PreviewController::navigateRequested, this, [this](int key, int modifiers) {
        emit Navigate(m_source, key, modifiers);
    });
    auto *watcher = new QDBusServiceWatcher(this);
    watcher->setConnection(QDBusConnection::sessionBus());
    watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &name) {
        if (name == m_owner) {
            m_owner.clear();
            m_source.clear();
        }
    });
    connect(this, &PreviewService::VisibilityChanged, watcher, [this, watcher](bool visible) {
        watcher->setWatchedServices(visible ? QStringList{m_owner} : QStringList{});
    });
}

PreviewService::~PreviewService()
{
    delete m_window;
}

QString PreviewService::caller() const
{
    return calledFromDBus() ? message().service() : QString();
}

bool PreviewService::owns(const QString &source) const
{
    return source == m_source && caller() == m_owner;
}

void PreviewService::apply(const QString &source, const QString &snapshot, bool activate)
{
    const auto data = QJsonDocument::fromJson(snapshot.toUtf8()).toVariant().toMap();
    const auto items = data.value("items").toList();
    if (items.isEmpty() || !m_window)
        return;
    const bool changedSource = m_source != source || m_owner != caller();
    m_source = source;
    m_owner = caller();
    emit VisibilityChanged(true);
    m_controller.present(items, data.value("currentIndex").toInt(), activate,
                         data.value("ratio").toReal());
    if (changedSource)
        m_controller.select(data.value("currentIndex").toInt());
}

void PreviewService::Toggle(const QString &source, const QString &snapshot)
{
    if (m_controller.visible()) {
        m_source = source;
        m_owner = caller();
        m_controller.close();
    } else {
        if (m_window && m_window->screen()) {
            const QRect available = m_window->screen()->availableGeometry();
            m_window->setPosition(available.center() - QPoint(m_window->width() / 2, m_window->height() / 2));
        }
        apply(source, snapshot, true);
    }
}

void PreviewService::Activate(const QString &source, const QString &snapshot)
{
    if (m_controller.visible())
        apply(source, snapshot, false);
}

void PreviewService::Update(const QString &source, const QString &snapshot)
{
    if (m_controller.visible() && owns(source))
        apply(source, snapshot, false);
}

void PreviewService::Release(const QString &source)
{
    if (!owns(source))
        return;
    m_owner.clear();
    m_source.clear();
}

void PreviewService::Key(const QString &source, int key, int modifiers)
{
    if (!owns(source) || !m_controller.visible() || !m_window)
        return;
    QKeyEvent event(QEvent::KeyPress, key, Qt::KeyboardModifiers(modifiers));
    QCoreApplication::sendEvent(m_window, &event);
}

bool PreviewService::IsVisible() const
{
    return m_controller.visible();
}
