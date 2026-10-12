// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyring.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>

static const QString s_service = QStringLiteral("org.freedesktop.secrets");
static const QString s_servicePath = QStringLiteral("/org/freedesktop/secrets");
static const QString s_serviceInterface = QStringLiteral("org.freedesktop.Secret.Service");
static const QString s_schema = QStringLiteral("com.cutefish.ServerPassword");

using Attributes = QMap<QString, QString>;

struct KeyringSecret
{
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};

Q_DECLARE_METATYPE(KeyringSecret)

static QDBusArgument &operator<<(QDBusArgument &argument, const KeyringSecret &secret)
{
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value << secret.contentType;
    argument.endStructure();
    return argument;
}

static const QDBusArgument &operator>>(const QDBusArgument &argument, KeyringSecret &secret)
{
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >> secret.contentType;
    argument.endStructure();
    return argument;
}

static void registerTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<KeyringSecret>();
        qDBusRegisterMetaType<Attributes>();
        return true;
    }();
    (void)registered;
}

static QDBusMessage call(const QString &path, const QString &interface, const QString &method,
                         const QVariantList &arguments)
{
    auto message = QDBusMessage::createMethodCall(s_service, path, interface, method);
    message.setArguments(arguments);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
}

static QString openSession()
{
    const QDBusMessage reply = call(s_servicePath, s_serviceInterface, QStringLiteral("OpenSession"),
                                    {QStringLiteral("plain"),
                                     QVariant::fromValue(QDBusVariant(QString()))});
    if (reply.type() != QDBusMessage::ReplyMessage)
        return QString();

    return reply.arguments().value(1).value<QDBusObjectPath>().path();
}

static void closeSession(const QString &session)
{
    call(session, QStringLiteral("org.freedesktop.Secret.Session"), QStringLiteral("Close"), {});
}

static Attributes attributesFor(const QUrl &url)
{
    Attributes attributes {
        {QStringLiteral("xdg:schema"), s_schema},
        {QStringLiteral("protocol"), url.scheme()},
        {QStringLiteral("server"), url.host().toLower()},
    };
    if (url.port() > 0)
        attributes.insert(QStringLiteral("port"), QString::number(url.port()));

    return attributes;
}

bool Keyring::find(const QUrl &url, Credentials *credentials)
{
    registerTypes();

    Attributes attributes = attributesFor(url);
    const QString user = !credentials->user.isEmpty() ? credentials->user : url.userName();
    if (!user.isEmpty())
        attributes.insert(QStringLiteral("user"), user);

    const QDBusMessage search = call(s_servicePath, s_serviceInterface,
                                     QStringLiteral("SearchItems"),
                                     {QVariant::fromValue(attributes)});
    if (search.type() != QDBusMessage::ReplyMessage)
        return false;

    const auto items = qdbus_cast<QList<QDBusObjectPath>>(search.arguments().value(0));
    if (items.isEmpty())
        return false;

    const QString session = openSession();
    if (session.isEmpty())
        return false;

    const QString item = items.first().path();
    const QDBusMessage secretReply = call(item, QStringLiteral("org.freedesktop.Secret.Item"),
                                          QStringLiteral("GetSecret"),
                                          {QVariant::fromValue(QDBusObjectPath(session))});

    auto properties = QDBusMessage::createMethodCall(s_service, item,
                                                     QStringLiteral("org.freedesktop.DBus.Properties"),
                                                     QStringLiteral("Get"));
    properties.setArguments({QStringLiteral("org.freedesktop.Secret.Item"),
                             QStringLiteral("Attributes")});
    const QDBusMessage attributesReply = QDBusConnection::sessionBus().call(properties,
                                                                            QDBus::Block, 5000);
    closeSession(session);

    if (secretReply.type() != QDBusMessage::ReplyMessage)
        return false;

    const auto secret = qdbus_cast<KeyringSecret>(secretReply.arguments().value(0));
    const auto stored = qdbus_cast<Attributes>(
            attributesReply.arguments().value(0).value<QDBusVariant>().variant());

    credentials->user = stored.value(QStringLiteral("user"), user);
    credentials->domain = stored.value(QStringLiteral("domain"));
    credentials->password = QString::fromUtf8(secret.value);
    return true;
}

void Keyring::store(const QUrl &url, const Credentials &credentials)
{
    registerTypes();

    const QDBusMessage alias = call(s_servicePath, s_serviceInterface, QStringLiteral("ReadAlias"),
                                    {QStringLiteral("default")});
    const QString collection = alias.arguments().value(0).value<QDBusObjectPath>().path();
    if (alias.type() != QDBusMessage::ReplyMessage || collection == QLatin1String("/"))
        return;

    const QString session = openSession();
    if (session.isEmpty())
        return;

    Attributes attributes = attributesFor(url);
    attributes.insert(QStringLiteral("user"), credentials.user);
    if (!credentials.domain.isEmpty())
        attributes.insert(QStringLiteral("domain"), credentials.domain);

    const QString account = credentials.domain.isEmpty()
            ? credentials.user
            : credentials.domain + QLatin1Char('\\') + credentials.user;
    const QVariantMap properties {
        {QStringLiteral("org.freedesktop.Secret.Item.Label"),
         QStringLiteral("%1 on %2").arg(account, url.host())},
        {QStringLiteral("org.freedesktop.Secret.Item.Attributes"), QVariant::fromValue(attributes)},
    };
    const KeyringSecret secret {QDBusObjectPath(session), {}, credentials.password.toUtf8(),
                                QStringLiteral("text/plain")};

    // A locked collection answers with a prompt, which is left unanswered.
    call(collection, QStringLiteral("org.freedesktop.Secret.Collection"),
         QStringLiteral("CreateItem"), {properties, QVariant::fromValue(secret), true});
    closeSession(session);
}
