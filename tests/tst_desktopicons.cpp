#include "model/foldermodel.h"

#include <QFile>
#include <QMimeDatabase>
#include <QTemporaryDir>
#include <QtTest>

class DesktopIconsTest : public QObject
{
    Q_OBJECT

private slots:

    void desktopIcon_data()
    {
        QTest::addColumn<QString>("icon");
        QTest::newRow("theme") << QStringLiteral("org.example.Application");
        QTest::newRow("absolute-path") << QStringLiteral("/opt/example/icons/app icon.png");
        QTest::newRow("missing") << QString();
    }

    void desktopIcon()
    {
        QFETCH(QString, icon);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("example.desktop")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray contents =
                "[Desktop Entry]\nType=Application\nName=Example\nExec=example\n"
                + (icon.isEmpty() ? QByteArray() : "Icon=" + icon.toUtf8() + '\n');
        QCOMPARE(file.write(contents), contents.size());
        file.close();

        FolderModel folder;
        folder.setUrl(QUrl::fromLocalFile(dir.path()).toString());
        QTRY_COMPARE(folder.count(), 1);
        const QModelIndex index = folder.index(0, 0);
        QVERIFY(folder.data(index, FolderModel::IsDesktopFileRole).toBool());
        const QString expected =
                icon.isEmpty() ? QMimeDatabase().mimeTypeForFile(file.fileName()).iconName() : icon;
        QCOMPARE(folder.data(index, FolderModel::IconNameRole).toString(), expected);
    }
};

QTEST_MAIN(DesktopIconsTest)
#include "tst_desktopicons.moc"
