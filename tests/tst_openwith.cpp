#include "mimetype/mimeappmanager.h"

#include <QDir>
#include <QFile>
#include <QMimeDatabase>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

class OpenWithTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultIsFirst_data()
    {
        QTest::addColumn<bool>("explicitDefault");
        QTest::newRow("without-saved-default") << false;
        QTest::newRow("configured-default") << true;
    }

    void defaultIsFirst()
    {
        QFETCH(bool, explicitDefault);
        QTemporaryDir config;
        QVERIFY(config.isValid());
        const QByteArray previousConfig = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        const auto restoreConfig = qScopeGuard([previousConfig] {
            if (previousConfig.isNull())
                qunsetenv("XDG_CONFIG_HOME");
            else
                qputenv("XDG_CONFIG_HOME", previousConfig);
        });

        QFile file(config.filePath("sample # text.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Sample text\n");
        file.close();

        MimeAppManager manager;
        manager.initApplications();
        const QStringList candidates = manager.getRecommendedAppsByFilePath(file.fileName());
        if (candidates.isEmpty())
            QSKIP("No installed text applications");

        const QString expected = explicitDefault ? candidates.last() : candidates.first();
        if (explicitDefault)
            QVERIFY(manager.setDefaultAppForFile(file.fileName(), expected));
        else
            QVERIFY(manager.getDefaultAppByFilePath(file.fileName()).isEmpty());

        const QVariantList applications = manager.recommendedApps(QUrl::fromLocalFile(file.fileName()));
        QVERIFY(!applications.isEmpty());
        QCOMPARE(applications.first().toMap().value("desktopFile").toString(), expected);
        for (int i = 1; i < applications.size(); ++i) {
            const QVariantMap app = applications.at(i).toMap();
            QVERIFY(app.value("desktopFile").toString() != expected);
        }
    }

    // A rescan rebuilds the application lists instead of adding to the last
    // one, so a type an application stopped listing is no longer offered.
    void rescanDropsStaleAssociations()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QByteArray previousHome = qgetenv("HOME");
        qputenv("HOME", home.path().toUtf8());
        const auto restoreHome = qScopeGuard([previousHome] {
            if (previousHome.isNull())
                qunsetenv("HOME");
            else
                qputenv("HOME", previousHome);
        });

        QVERIFY(QDir(home.path()).mkpath(".local/share/applications"));
        const QString path = home.filePath(".local/share/applications/cutefish-test-viewer.desktop");
        const auto writeDesktopFile = [&path](const QStringList &mimeTypes) {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                return false;

            file.write("[Desktop Entry]\nType=Application\nName=Cutefish Test Viewer\n"
                       "Exec=cutefish-test-viewer %f\n");
            file.write("MimeType=" + mimeTypes.join(QLatin1Char(';')).toUtf8() + "\n");
            return true;
        };

        const QList<QMimeType> types = QMimeDatabase().allMimeTypes();
        QStringList names;
        for (const QMimeType &type : types)
            names.append(type.name());
        QVERIFY(writeDesktopFile(names));

        MimeAppManager manager;
        manager.initApplications();

        // Types no installed application handles drop out of the next scan.
        QList<QMimeType> onlyOurs;
        for (const QMimeType &type : types) {
            if (manager.getRecommendedAppsByMimeType(type) == QStringList{path})
                onlyOurs.append(type);
        }
        QVERIFY(!onlyOurs.isEmpty());

        QVERIFY(writeDesktopFile({}));
        manager.initApplications();
        for (const QMimeType &type : std::as_const(onlyOurs)) {
            QVERIFY2(!manager.getRecommendedAppsByMimeType(type).contains(path),
                     qPrintable(type.name()));
        }
    }
};

QTEST_GUILESS_MAIN(OpenWithTest)
#include "tst_openwith.moc"
