#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

#include <signal.h>

class DBusActivationTest : public QObject
{
    Q_OBJECT

private slots:
    void coldStart_data()
    {
        QTest::addColumn<QString>("method");
        QTest::newRow("items") << QStringLiteral("ShowItems");
        QTest::newRow("folders") << QStringLiteral("ShowFolders");
    }

    void coldStart()
    {
        QFETCH(QString, method);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        QFile source(QStringLiteral(SERVICE_TEMPLATE));
        QVERIFY(source.open(QIODevice::ReadOnly));
        QByteArray service = source.readAll();
        service.replace("@CMAKE_INSTALL_FULL_BINDIR@",
                        QFileInfo(QStringLiteral(FILEMANAGER_EXECUTABLE)).absolutePath().toUtf8());
        QFile entry(directory.filePath("com.cutefish.FileManager1.service"));
        QVERIFY(entry.open(QIODevice::WriteOnly));
        QCOMPARE(entry.write(service), service.size());
        entry.close();

        QFile config(directory.filePath("session.conf"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray configuration = QStringLiteral(
                "<busconfig><type>session</type><listen>unix:dir=%1</listen>"
                "<servicedir>%1</servicedir><policy context=\"default\">"
                "<allow own=\"*\"/><allow send_destination=\"*\"/>"
                "<allow receive_sender=\"*\"/></policy></busconfig>")
                .arg(directory.path().toHtmlEscaped()).toUtf8();
        QCOMPARE(config.write(configuration), configuration.size());
        config.close();

        QProcess bus;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("XDG_CONFIG_HOME", directory.filePath("config"));
        environment.insert("XDG_DATA_HOME", directory.filePath("data"));
        bus.setProcessEnvironment(environment);
        // The activated file manager inherits this stderr; an unread pipe would stall it.
        bus.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        bus.start(QStringLiteral(DBUS_DAEMON_EXECUTABLE),
                  {"--nofork", "--print-address=1", "--config-file=" + config.fileName()});
        QVERIFY(bus.waitForStarted());
        const auto stopBus = qScopeGuard([&] {
            bus.terminate();
            if (!bus.waitForFinished(3000)) {
                bus.kill();
                bus.waitForFinished();
            }
        });
        QTRY_VERIFY(bus.canReadLine());
        const QString address = QString::fromUtf8(bus.readLine()).trimmed();
        const QString connectionName = QStringLiteral("activation-test");
        auto connection = QDBusConnection::connectToBus(address, connectionName);
        const auto disconnect = qScopeGuard([&] {
            if (connection.interface()) {
                const auto pid = connection.interface()->servicePid("org.freedesktop.FileManager1");
                if (pid.isValid())
                    ::kill(pid.value(), SIGTERM);
            }
            QDBusConnection::disconnectFromBus(connectionName);
        });
        QVERIFY(connection.isConnected());
        const auto registered = connection.interface()->isServiceRegistered("org.freedesktop.FileManager1");
        QVERIFY(registered.isValid());
        QVERIFY(!registered.value());

        QFile item(directory.filePath("Screenshot #1.png"));
        QVERIFY(item.open(QIODevice::WriteOnly));
        item.close();
        const QString path = method == "ShowItems" ? item.fileName() : directory.path();
        auto message = QDBusMessage::createMethodCall(
                "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
                "org.freedesktop.FileManager1", method);
        message << QStringList {QUrl::fromLocalFile(path).toString()} << QString();
        const auto reply = connection.call(message, QDBus::Block, 10000);
        QVERIFY2(reply.type() == QDBusMessage::ReplyMessage, qPrintable(reply.errorMessage()));
        QVERIFY(connection.interface()->isServiceRegistered("org.freedesktop.FileManager1").value());
    }
};

QTEST_GUILESS_MAIN(DBusActivationTest)
#include "tst_dbusactivation.moc"
