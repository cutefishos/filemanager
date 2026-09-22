#include "previewclient.h"
#include "previewcontroller.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QUuid>

namespace {
constexpr auto serviceName = "com.cutefish.FilePreview";
constexpr auto servicePath = "/com/cutefish/FilePreview";
}

PreviewClient::PreviewClient(QObject *parent)
    : QObject(parent)
    , m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    auto bus = QDBusConnection::sessionBus();
    bus.connect(serviceName, servicePath, serviceName, "VisibilityChanged", this, SLOT(visibilityChanged(bool)));
    bus.connect(serviceName, servicePath, serviceName, "Navigate", this, SLOT(navigate(QString,int,int)));
    bus.connect(serviceName, servicePath, serviceName, "RestoreFocus", this, SLOT(restoreFocus(QString)));
    auto *watcher = new QDBusServiceWatcher(serviceName, bus,
            QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        visibilityChanged(false);
    });
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, [this] {
        if (m_model && m_view)
            send("Update", {m_id, snapshot()});
    });
}

PreviewClient::~PreviewClient()
{
    send("Release", {m_id});
}

bool PreviewClient::previewVisible() const
{
    return m_previewVisible;
}

QString PreviewClient::snapshot() const
{
    return QString::fromUtf8(QJsonDocument::fromVariant(PreviewController::snapshot(m_model, m_view))
                            .toJson(QJsonDocument::Compact));
}

void PreviewClient::send(const QString &method, const QVariantList &arguments, bool autoStart)
{
    auto message = QDBusMessage::createMethodCall(serviceName, servicePath, serviceName, method);
    message.setArguments(arguments);
    message.setAutoStartService(autoStart);
    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call, autoStart] {
        const QDBusPendingReply<> reply = *call;
        if (autoStart && reply.isError()) {
            visibilityChanged(false);
            if (m_model)
                emit m_model->notification(tr("Could not open preview: %1").arg(reply.error().message()));
        }
        call->deleteLater();
    });
}

void PreviewClient::attach(FolderModel *model, QQuickItem *view)
{
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    if (m_view) {
        m_view->removeEventFilter(this);
        disconnect(m_view, nullptr, this, nullptr);
    }
    m_model = model;
    m_view = view;
    if (!m_model || !m_view)
        return;
    m_view->installEventFilter(this);
    connect(m_view, &QQuickItem::activeFocusChanged, this, &PreviewClient::activate);
    connect(m_view, &QQuickItem::visibleChanged, this, &PreviewClient::activate);
    connect(m_view, &QQuickItem::windowChanged, this, &PreviewClient::bindWindow);
    connect(m_model, &FolderModel::selectionCountChanged, this, [this] { m_updateTimer.start(); });
    connect(m_model, &FolderModel::urlChanged, this, [this] { m_updateTimer.start(); });
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this] { m_updateTimer.start(); });
    bindWindow();
    auto request = QDBusMessage::createMethodCall(serviceName, servicePath, serviceName, "IsVisible");
    request.setAutoStartService(false);
    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call] {
        const QDBusPendingReply<bool> reply = *call;
        if (reply.isValid())
            visibilityChanged(reply.value());
        call->deleteLater();
    });
}

void PreviewClient::bindWindow()
{
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
    m_window = m_view ? m_view->window() : nullptr;
    if (m_window)
        connect(m_window, &QWindow::activeChanged, this, &PreviewClient::activate);
    activate();
}

void PreviewClient::activate()
{
    if (m_model && m_view && m_view->isVisible() && m_window && m_window->isActive())
        send("Activate", {m_id, snapshot()});
}

void PreviewClient::toggle()
{
    if (!m_model || !m_view || m_view->property("editor").value<QObject *>())
        return;
    // Keep the desktop selection when the preview window takes keyboard focus.
    visibilityChanged(true);
    send("Toggle", {m_id, snapshot()}, true);
}

void PreviewClient::visibilityChanged(bool visible)
{
    if (m_previewVisible == visible)
        return;
    m_previewVisible = visible;
    emit previewVisibleChanged();
}

void PreviewClient::navigate(const QString &source, int key, int modifiers)
{
    if (source != m_id || !m_view)
        return;
    m_navigating = true;
    QKeyEvent event(QEvent::KeyPress, key, Qt::KeyboardModifiers(modifiers));
    QCoreApplication::sendEvent(m_view, &event);
    m_navigating = false;
}

void PreviewClient::restoreFocus(const QString &source)
{
    if (source != m_id || !m_view || !m_view->isVisible())
        return;
    m_view->forceActiveFocus();
    if (m_window)
        m_window->requestActivate();
}

bool PreviewClient::eventFilter(QObject *object, QEvent *event)
{
    if (!m_previewVisible || m_navigating || event->type() != QEvent::KeyPress)
        return QObject::eventFilter(object, event);
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Escape
        || (m_model && m_model->selectionCount() > 1 && key->modifiers() == Qt::NoModifier
            && (key->key() == Qt::Key_Left || key->key() == Qt::Key_Right))) {
        send("Key", {m_id, key->key(), int(key->modifiers())});
        return true;
    }
    return QObject::eventFilter(object, event);
}
