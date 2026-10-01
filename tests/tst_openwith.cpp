#include "mimetype/mimeappmanager.h"

#include <applicationregistry.h>

#include <QFile>
#include <QMimeDatabase>
#include <QTemporaryDir>
#include <QtTest>

namespace {
DesktopEntryData textApp(const QString &id, const QString &path, const QStringList &mimeTypes)
{
    DesktopEntryData data;
    const QByteArray contents = "[Desktop Entry]\nType=Application\nName=" + id.toUtf8()
            + "\nExec=" + id.toUtf8() + " %f\nMimeType=" + mimeTypes.join(';').toUtf8() + ";\n";
    DesktopEntry::parse(id, path, contents, &data);
    return data;
}
} // namespace

class OpenWithTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        QTRY_VERIFY(!ApplicationRegistry::instance()->loading());
    }

    void defaultIsFirst_data()
    {
        QTest::addColumn<bool>("explicitDefault");
        QTest::newRow("without-saved-default") << false;
        QTest::newRow("configured-default") << true;
    }

    void defaultIsFirst()
    {
        QFETCH(bool, explicitDefault);
        QFile::remove(m_config.filePath("mimeapps.list"));

        QFile file(m_config.filePath("sample # text.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Sample text\n");
        file.close();

        ApplicationRegistry::instance()->applyScan(
                {textApp("first-editor", "/applications/first-editor.desktop", {"text/plain"}),
                 textApp("second-editor", "/applications/second-editor.desktop", {"text/plain"})});

        MimeAppManager manager;
        const QStringList candidates = manager.getRecommendedAppsByFilePath(file.fileName());
        QCOMPARE(candidates.size(), 2);

        const QString expected = explicitDefault ? candidates.last() : candidates.first();
        if (explicitDefault)
            QVERIFY(manager.setDefaultAppForFile(file.fileName(), expected));
        else
            QVERIFY(manager.getDefaultAppByFilePath(file.fileName()).isEmpty());

        const QVariantList applications = manager.recommendedApps(QUrl::fromLocalFile(file.fileName()));
        QCOMPARE(applications.size(), 2);
        QCOMPARE(applications.first().toMap().value("desktopFile").toString(), expected);
        QVERIFY(applications.last().toMap().value("desktopFile").toString() != expected);

        if (explicitDefault) {
            QFile list(m_config.filePath("mimeapps.list"));
            QVERIFY(list.open(QIODevice::ReadOnly));
            QVERIFY(list.readAll().contains("text/plain=second-editor.desktop;"));
        }
    }

    // The lists follow the registry: a type an application stopped listing is no
    // longer offered.
    void followsTheRegistry()
    {
        const QString path = QStringLiteral("/applications/cutefish-test-viewer.desktop");
        const QMimeType type = QMimeDatabase().mimeTypeForName(QStringLiteral("image/x-tga"));
        QVERIFY(type.isValid());

        MimeAppManager manager;
        ApplicationRegistry::instance()->applyScan(
                {textApp("cutefish-test-viewer", path, {type.name()})});
        QCOMPARE(manager.getRecommendedAppsByMimeType(type), QStringList {path});

        ApplicationRegistry::instance()->applyScan(
                {textApp("cutefish-test-viewer", path, {QStringLiteral("text/plain")})});
        QVERIFY(!manager.getRecommendedAppsByMimeType(type).contains(path));
    }

    // Parent types stand in when nothing handles the type itself.
    void fallsBackToParentTypes()
    {
        const QMimeType type = QMimeDatabase().mimeTypeForName(QStringLiteral("text/x-csrc"));
        QVERIFY(type.isValid());

        MimeAppManager manager;
        ApplicationRegistry::instance()->applyScan(
                {textApp("plain-editor", "/applications/plain-editor.desktop", {"text/plain"})});
        QCOMPARE(manager.getRecommendedAppsByMimeType(type),
                 QStringList {"/applications/plain-editor.desktop"});
    }

private:
    QTemporaryDir m_config;
};

QTEST_GUILESS_MAIN(OpenWithTest)
#include "tst_openwith.moc"
