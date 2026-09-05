#include "../model/placesmodel.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class PlacesModelTest : public QObject
{
    Q_OBJECT

private slots:
    void favorites()
    {
        QTemporaryDir config;
        QTemporaryDir folders;
        QVERIFY(config.isValid());
        QVERIFY(folders.isValid());
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        QSettings settings(QStringLiteral("cutefish"), QStringLiteral("filemanager-sidebar"));
        settings.setValue(QStringLiteral("favorites"), QStringList());
        settings.sync();

        QDir dir(folders.path());
        QVERIFY(dir.mkdir("first"));
        QVERIFY(dir.mkdir("second"));
        const QUrl first = QUrl::fromLocalFile(dir.filePath("first"));
        const QUrl second = QUrl::fromLocalFile(dir.filePath("second"));
        PlacesModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(model.favoriteCount(), 0);
        QVERIFY(!model.canAddFavorites({}));
        QVERIFY(!model.addFavorites({QUrl("https://example.com")}, 0));
        QVERIFY(!model.addFavorites({first, QUrl::fromLocalFile(dir.filePath("missing"))}, 0));
        QCOMPARE(model.favoriteCount(), 0);
        QVERIFY(model.addFavorites({first, second}, 0));
        QCOMPARE(model.favoriteCount(), 2);
        QVERIFY(model.addFavorites({first}, 0));
        QCOMPARE(model.favoriteCount(), 2);
        QVERIFY(!model.moveFavorite(2, 0));
        QVERIFY(!model.moveFavorite(0, 3));
        QVERIFY(model.moveFavorite(0, 2));
        QCOMPARE(model.get(0).value("url").toUrl(), second);
        QVERIFY(model.moveFavorite(1, 0));
        QCOMPARE(model.get(0).value("url").toUrl(), first);
        QVERIFY(model.moveFavorite(0, 1));
        model.removeFavorite(0);
        QCOMPARE(model.favoriteCount(), 1);
        QVERIFY(QFileInfo(first.toLocalFile()).isDir());
        {
            PlacesModel restored;
            QCOMPARE(restored.favoriteCount(), 1);
            QCOMPARE(restored.get(0).value("url").toUrl(), second);
        }
        model.removeFavorite(0);
        PlacesModel empty;
        QCOMPARE(empty.favoriteCount(), 0);
    }
};

QTEST_GUILESS_MAIN(PlacesModelTest)
#include "tst_placesmodel.moc"
