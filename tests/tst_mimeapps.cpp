#include "mimetype/mimeapps.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class MimeAppsTest : public QObject
{
    Q_OBJECT
private slots:

    void preservesAssociations()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("mimeapps.list");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# Keep this comment\n[Default Applications]\nimage/png=image.desktop;\n"
                   "[Added Associations]\ntext/plain=old.desktop;new.desktop;\n"
                   "[Removed Associations]\ntext/plain=new.desktop;blocked.desktop;\n");
        file.close();
        QVERIFY(MimeApps::setDefault(path, "text/plain", "new.desktop"));
        QVERIFY(MimeApps::setDefault(path, "text/plain", "new.desktop"));
        QCOMPARE(MimeApps::associations(path, "Default Applications", "text/plain"),
                 QStringList {"new.desktop"});
        QCOMPARE(MimeApps::associations(path, "Default Applications", "image/png"),
                 QStringList {"image.desktop"});
        QCOMPARE(MimeApps::associations(path, "Added Associations", "text/plain"),
                 QStringList({"new.desktop", "old.desktop"}));
        QCOMPARE(MimeApps::associations(path, "Removed Associations", "text/plain"),
                 QStringList {"blocked.desktop"});
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto contents = file.readAll();
        QVERIFY(contents.contains("# Keep this comment"));
        QVERIFY(contents.contains("text/plain=new.desktop;"));
        QVERIFY(!contents.contains("text%2Fplain"));
        QVERIFY(!MimeApps::setDefault(path, "text/plain", "bad;entry.desktop"));
        QVERIFY(!MimeApps::setDefault(dir.path(), "text/plain", "new.desktop"));
    }
};
QTEST_GUILESS_MAIN(MimeAppsTest)
#include "tst_mimeapps.moc"
