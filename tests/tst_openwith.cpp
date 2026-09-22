#include "mimetype/mimeappmanager.h"

#include <QFile>
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
};

QTEST_GUILESS_MAIN(OpenWithTest)
#include "tst_openwith.moc"
