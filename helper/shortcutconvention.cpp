#include "shortcutconvention.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>

static constexpr auto kService = "com.cutefish.Keyboard1";
static constexpr auto kPath = "/com/cutefish/Keyboard1";
static constexpr auto kInterface = "com.cutefish.Keyboard1";

ShortcutConvention *ShortcutConvention::self()
{
    static ShortcutConvention convention;
    return &convention;
}

ShortcutConvention::ShortcutConvention(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    auto *watcher = new QDBusServiceWatcher(QLatin1String(kService), bus,
                                            QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &ShortcutConvention::refresh);
    bus.connect(QLatin1String(kService), QLatin1String(kPath), QLatin1String(kInterface),
                QStringLiteral("shortcutConventionChanged"), this, SLOT(refresh()));
    refresh();
}

void ShortcutConvention::setCommandKeys(bool commandKeys)
{
    m_commandKeys = commandKeys;
}

void ShortcutConvention::refresh()
{
    QDBusMessage message = QDBusMessage::createMethodCall(
            QLatin1String(kService), QLatin1String(kPath),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    message << QLatin1String(kInterface) << QStringLiteral("shortcutConvention");
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message),
                                                this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QDBusVariant> reply = *call;
        if (reply.isValid())
            setCommandKeys(reply.value().variant().toString() == QLatin1String("command"));
        call->deleteLater();
    });
}
