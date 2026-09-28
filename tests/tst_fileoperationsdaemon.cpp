#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

class FileOperationsDaemonTest : public QObject
{
    Q_OBJECT

private slots:
    void operationOutlivesSubmittingClient()
    {
        QProcess daemon;
        daemon.start(QCoreApplication::applicationDirPath()
                             + QStringLiteral("/cutefish-filemanager-daemon"));
        QVERIFY2(daemon.waitForStarted(), qPrintable(daemon.errorString()));

        QTemporaryDir sourceDirectory;
        QTemporaryDir destinationDirectory;
        QVERIFY(sourceDirectory.isValid());
        QVERIFY(destinationDirectory.isValid());

        QFile source(sourceDirectory.filePath(QStringLiteral("payload.bin")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        const QByteArray block(1024 * 1024, 'x');
        for (int index = 0; index < 16; ++index)
            QCOMPARE(source.write(block), qint64(block.size()));
        source.close();

        QString taskPath;
        {
            QDBusInterface manager(QStringLiteral("com.cutefish.FileOperations"),
                                   QStringLiteral("/com/cutefish/FileOperations"),
                                   QStringLiteral("com.cutefish.FileOperations.Manager1"),
                                   QDBusConnection::sessionBus());
            QTRY_VERIFY_WITH_TIMEOUT(manager.isValid(), 3000);
            QDBusReply<QDBusObjectPath> submitted = manager.call(
                    QStringLiteral("Submit"), 0,
                    QStringList {QUrl::fromLocalFile(source.fileName()).toString()},
                    QUrl::fromLocalFile(destinationDirectory.path()).toString(), 4);
            QVERIFY2(submitted.isValid(), qPrintable(submitted.error().message()));
            taskPath = submitted.value().path();
        }

        QDBusInterface task(QStringLiteral("com.cutefish.FileOperations"), taskPath,
                            QStringLiteral("com.cutefish.FileOperations.Task1"),
                            QDBusConnection::sessionBus());
        QVariantMap snapshot;
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            QDBusReply<QVariantMap> reply = task.call(QStringLiteral("GetSnapshot"));
            if (!reply.isValid())
                return false;
            snapshot = reply.value();
            return snapshot.value(QStringLiteral("finished")).toBool();
        })(), 10000);

        QCOMPARE(snapshot.value(QStringLiteral("errorCode")).toInt(), 0);
        QFile copied(destinationDirectory.filePath(QStringLiteral("payload.bin")));
        QVERIFY(copied.exists());
        QCOMPARE(copied.size(), source.size());

        daemon.terminate();
        QVERIFY(daemon.waitForFinished(3000));
    }

    void liveTasksAreDiscoverable()
    {
        QProcess daemon;
        daemon.start(QCoreApplication::applicationDirPath()
                             + QStringLiteral("/cutefish-filemanager-daemon"));
        QVERIFY2(daemon.waitForStarted(), qPrintable(daemon.errorString()));

        QDBusInterface manager(QStringLiteral("com.cutefish.FileOperations"),
                               QStringLiteral("/com/cutefish/FileOperations"),
                               QStringLiteral("com.cutefish.FileOperations.Manager1"),
                               QDBusConnection::sessionBus());
        QTRY_VERIFY_WITH_TIMEOUT(manager.isValid(), 3000);

        QTemporaryDir directory;
        QFile source(directory.filePath(QStringLiteral("source")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("new");
        source.close();
        QFile destination(directory.filePath(QStringLiteral("destination")));
        QVERIFY(destination.open(QIODevice::WriteOnly));
        destination.write("old");
        destination.close();

        QDBusReply<QDBusObjectPath> submitted = manager.call(
                QStringLiteral("Submit"), 5,
                QStringList {QUrl::fromLocalFile(source.fileName()).toString()},
                QUrl::fromLocalFile(destination.fileName()).toString(), 4);
        QVERIFY2(submitted.isValid(), qPrintable(submitted.error().message()));
        const QString taskPath = submitted.value().path();

        QDBusInterface task(QStringLiteral("com.cutefish.FileOperations"), taskPath,
                            QStringLiteral("com.cutefish.FileOperations.Task1"),
                            QDBusConnection::sessionBus());
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const QDBusReply<QVariantMap> reply = task.call(QStringLiteral("GetSnapshot"));
            return reply.isValid()
                    && reply.value().value(QStringLiteral("waitingForConflict")).toBool();
        })(), 3000);

        const QDBusReply<QVariantMap> waiting = task.call(QStringLiteral("GetSnapshot"));
        QVERIFY(waiting.isValid());
        const QVariant progressValue = waiting.value().value(QStringLiteral("progress"));
        const QVariantMap progress = progressValue.metaType()
                        == QMetaType::fromType<QDBusArgument>()
                ? qdbus_cast<QVariantMap>(progressValue.value<QDBusArgument>())
                : progressValue.toMap();
        QVERIFY(!progress.isEmpty());
        QVERIFY(progress.value(QStringLiteral("totalsKnown")).toBool());
        QCOMPARE(progress.value(QStringLiteral("totalItems")).toULongLong(), quint64(1));

        const QDBusReply<QStringList> paths = manager.call(QStringLiteral("TaskPaths"));
        QVERIFY(paths.isValid());
        QVERIFY(paths.value().contains(taskPath));

        task.call(QStringLiteral("ResolveConflict"), 1, false);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const QDBusReply<QVariantMap> reply = task.call(QStringLiteral("GetSnapshot"));
            return reply.isValid() && reply.value().value(QStringLiteral("finished")).toBool();
        })(), 3000);

        daemon.terminate();
        QVERIFY(daemon.waitForFinished(3000));
    }

    void finishedTasksLeaveTheBus()
    {
        QProcess daemon;
        daemon.start(QCoreApplication::applicationDirPath()
                             + QStringLiteral("/cutefish-filemanager-daemon"));
        QVERIFY2(daemon.waitForStarted(), qPrintable(daemon.errorString()));

        QDBusInterface manager(QStringLiteral("com.cutefish.FileOperations"),
                               QStringLiteral("/com/cutefish/FileOperations"),
                               QStringLiteral("com.cutefish.FileOperations.Manager1"),
                               QDBusConnection::sessionBus());
        QTRY_VERIFY_WITH_TIMEOUT(manager.isValid(), 3000);

        QTemporaryDir directory;
        QDBusReply<QDBusObjectPath> submitted = manager.call(
                QStringLiteral("Submit"), 4, QStringList(),
                QUrl::fromLocalFile(directory.filePath(QStringLiteral("created"))).toString(), 0);
        QVERIFY2(submitted.isValid(), qPrintable(submitted.error().message()));
        const QString taskPath = submitted.value().path();

        QDBusInterface task(QStringLiteral("com.cutefish.FileOperations"), taskPath,
                            QStringLiteral("com.cutefish.FileOperations.Task1"),
                            QDBusConnection::sessionBus());
        // A client attaching just after the task ended still reads its result.
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const QDBusReply<QVariantMap> reply = task.call(QStringLiteral("GetSnapshot"));
            return reply.isValid() && reply.value().value(QStringLiteral("finished")).toBool();
        })(), 3000);
        QVERIFY(QFileInfo(directory.filePath(QStringLiteral("created"))).isDir());

        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const QDBusReply<QVariantMap> reply = task.call(QStringLiteral("GetSnapshot"));
            return !reply.isValid();
        })(), 15000);

        daemon.terminate();
        QVERIFY(daemon.waitForFinished(3000));
    }

    void conflictOfVanishedSubmitterIsCanceled()
    {
        QProcess daemon;
        daemon.start(QCoreApplication::applicationDirPath()
                             + QStringLiteral("/cutefish-filemanager-daemon"));
        QVERIFY2(daemon.waitForStarted(), qPrintable(daemon.errorString()));

        QTemporaryDir directory;
        QFile source(directory.filePath(QStringLiteral("source")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("new");
        source.close();
        QFile destination(directory.filePath(QStringLiteral("destination")));
        QVERIFY(destination.open(QIODevice::WriteOnly));
        destination.write("old");
        destination.close();

        QString taskPath;
        {
            const QDBusConnection submitter = QDBusConnection::connectToBus(
                    QDBusConnection::SessionBus, QStringLiteral("submitter"));
            QVERIFY(submitter.isConnected());
            QDBusInterface manager(QStringLiteral("com.cutefish.FileOperations"),
                                   QStringLiteral("/com/cutefish/FileOperations"),
                                   QStringLiteral("com.cutefish.FileOperations.Manager1"),
                                   submitter);
            QTRY_VERIFY_WITH_TIMEOUT(manager.isValid(), 3000);
            QDBusReply<QDBusObjectPath> submitted = manager.call(
                    QStringLiteral("Submit"), 5,
                    QStringList {QUrl::fromLocalFile(source.fileName()).toString()},
                    QUrl::fromLocalFile(destination.fileName()).toString(), 4);
            QVERIFY2(submitted.isValid(), qPrintable(submitted.error().message()));
            taskPath = submitted.value().path();
        }

        QDBusInterface task(QStringLiteral("com.cutefish.FileOperations"), taskPath,
                            QStringLiteral("com.cutefish.FileOperations.Task1"),
                            QDBusConnection::sessionBus());
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const QDBusReply<QVariantMap> reply = task.call(QStringLiteral("GetSnapshot"));
            return reply.isValid()
                    && reply.value().value(QStringLiteral("waitingForConflict")).toBool();
        })(), 3000);

        // The submitter leaves the bus without answering the conflict.
        QDBusConnection::disconnectFromBus(QStringLiteral("submitter"));
        QVariantMap snapshot;
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
            const QDBusReply<QVariantMap> reply = task.call(QStringLiteral("GetSnapshot"));
            if (!reply.isValid())
                return false;
            snapshot = reply.value();
            return snapshot.value(QStringLiteral("finished")).toBool();
        })(), 3000);
        QVERIFY(snapshot.value(QStringLiteral("canceled")).toBool());
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("old"));
        QVERIFY(QFileInfo::exists(source.fileName()));

        daemon.terminate();
        QVERIFY(daemon.waitForFinished(3000));
    }
};

QTEST_GUILESS_MAIN(FileOperationsDaemonTest)
#include "tst_fileoperationsdaemon.moc"
