#include "../helper/fm.h"
#include "../model/pathbarmodel.h"
#include "../model/placesmodel.h"
#include "preferences.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class PlacesModelTest : public QObject
{
    Q_OBJECT

private slots:

    // Runs first: the names are read once per process.
    void standardFolderNames()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        qputenv("HOME", home.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", home.filePath(".config").toUtf8());
        QVERIFY(QDir(home.path()).mkpath(".config"));
        QVERIFY(QDir(home.path()).mkpath(QStringLiteral("Schreibtisch/notes")));
        QFile dirs(home.filePath(".config/user-dirs.dirs"));
        QVERIFY(dirs.open(QIODevice::WriteOnly));
        dirs.write("XDG_DESKTOP_DIR=\"$HOME/Schreibtisch\"\nXDG_TEMPLATES_DIR=\"$HOME/\"\n");
        dirs.close();

        const QString desktop = home.filePath(QStringLiteral("Schreibtisch"));
        QCOMPARE(Fm::standardFolderName(desktop), QStringLiteral("Desktop"));
        QCOMPARE(Fm::standardFolderName(desktop + "/"), QStringLiteral("Desktop"));
        // A disabled directory points at home, which keeps its own name.
        QCOMPARE(Fm::standardFolderName(home.path()), QStringLiteral("Home"));
        QVERIFY(Fm::standardFolderName(desktop + "/notes").isEmpty());

        PathBarModel pathBar;
        pathBar.setUrl(QUrl::fromLocalFile(desktop + "/notes").toString());
        QCOMPARE(pathBar.rowCount(), 3);
        QCOMPARE(pathBar.index(0, 0).data(PathBarModel::NameRole).toString(),
                 QStringLiteral("Home"));
        QCOMPARE(pathBar.index(1, 0).data(PathBarModel::NameRole).toString(),
                 QStringLiteral("Desktop"));
        QCOMPARE(pathBar.index(2, 0).data(PathBarModel::NameRole).toString(),
                 QStringLiteral("notes"));
    }

    void favorites()
    {
        QTemporaryDir config;
        QTemporaryDir folders;
        QVERIFY(config.isValid());
        QVERIFY(folders.isValid());
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        QSettings settings(Preferences::file(QStringLiteral("filemanager")), QSettings::IniFormat);
        settings.setValue(QStringLiteral("Sidebar/favorites"), QStringList());
        settings.sync();

        QDir dir(folders.path());
        QVERIFY(dir.mkdir("first"));
        QVERIFY(dir.mkdir("second"));
        const QUrl first = QUrl::fromLocalFile(dir.filePath("first"));
        const QUrl second = QUrl::fromLocalFile(dir.filePath("second"));
        PlacesModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(model.favoriteCount(), 0);
        const auto checkTrash = [](const PlacesModel &places) {
            const QModelIndex trash = places.index(places.favoriteCount(), 0);
            QCOMPARE(trash.data(PlacesModel::UrlRole).toUrl(), QUrl("trash:///"));
            QVERIFY(trash.data(PlacesModel::CategoryRole).toString().isEmpty());
            QVERIFY(!trash.data(PlacesModel::IsDeviceRole).toBool());
        };
        checkTrash(model);
        QVERIFY(!model.canAddFavorites({}));
        QVERIFY(!model.addFavorites({QUrl("https://example.com")}, 0));
        QVERIFY(!model.addFavorites({first, QUrl::fromLocalFile(dir.filePath("missing"))}, 0));
        QCOMPARE(model.favoriteCount(), 0);
        QVERIFY(model.addFavorites({first, second}, 0));
        QCOMPARE(model.favoriteCount(), 2);
        checkTrash(model);
        QVERIFY(model.addFavorites({first}, 0));
        QCOMPARE(model.favoriteCount(), 2);
        checkTrash(model);
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
            checkTrash(restored);
            QCOMPARE(restored.get(0).value("url").toUrl(), second);
        }
        model.removeFavorite(0);
        PlacesModel empty;
        QCOMPARE(empty.favoriteCount(), 0);
        checkTrash(empty);
    }
};

QTEST_GUILESS_MAIN(PlacesModelTest)
#include "tst_placesmodel.moc"
