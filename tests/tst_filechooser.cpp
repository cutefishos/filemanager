#include "chooser/filechooser.h"
#include "qmltypes.h"

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class FileChooserTest : public QObject
{
    Q_OBJECT

    static void writeFile(const QString &path)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("example\n");
    }

    static int rowFor(FileChooser &chooser, const QString &name)
    {
        for (int row = 0; row < chooser.rowCount(); ++row) {
            if (chooser.index(row, 0).data(FolderModel::FileNameRole).toString() == name)
                return row;
        }
        return -1;
    }

private slots:

    void openMultipleAndFilter()
    {
        QTemporaryDir directory;
        writeFile(directory.filePath("first.txt"));
        writeFile(directory.filePath("second.txt"));
        writeFile(directory.filePath("photo.png"));
        QDir(directory.path()).mkdir("Documents");
        auto request = QJsonDocument::fromJson(R"({"mode":"OpenFile", "multiple":true,
            "filters":[{"name":"Text", "rules":[{"type":0,"value":"*.txt"}]}]})")
                               .object();
        request.insert("current_folder", directory.path());
        FileChooser chooser(request);
        QTRY_COMPARE(chooser.rowCount(), 3);
        QCOMPARE(rowFor(chooser, "photo.png"), -1);
        QVERIFY(rowFor(chooser, "Documents") >= 0);
        chooser.select(rowFor(chooser, "first.txt"));
        chooser.select(rowFor(chooser, "second.txt"), Qt::ControlModifier);
        QSignalSpy completed(&chooser, &FileChooser::finished);
        chooser.accept();
        QCOMPARE(completed.size(), 1);
        const auto result = completed.first().first().toJsonObject();
        QCOMPARE(result.value("response").toInt(), 0);
        QCOMPARE(result.value("uris").toArray().size(), 2);
        chooser.cancel();
        QCOMPARE(completed.size(), 1);
    }

    void namesSortNumerically()
    {
        QTemporaryDir directory;
        for (const QString &name : {"page10.txt", "page2.txt", "page1.txt"})
            writeFile(directory.filePath(name));
        FileChooser chooser({{"mode", "OpenFile"}, {"current_folder", directory.path()}});
        chooser.setSortMode(Cutefish::IO::DirectoryModel::Name);
        chooser.setSortDesc(false);
        QTRY_COMPARE(chooser.rowCount(), 3);
        QTRY_COMPARE(rowFor(chooser, "page1.txt"), 0);
        QCOMPARE(rowFor(chooser, "page2.txt"), 1);
        QCOMPARE(rowFor(chooser, "page10.txt"), 2);
    }

    void mimeAndGlobAreAlternatives()
    {
        QTemporaryDir directory;
        writeFile(directory.filePath("document.txt"));
        writeFile(directory.filePath("photo.png"));
        writeFile(directory.filePath("archive.zip"));
        auto request = QJsonDocument::fromJson(R"({"mode":"OpenFile",
            "filters":[{"name":"Documents and images", "rules":[
            {"type":0,"value":"*.txt"},{"type":1,"value":"image/png"}]}]})")
                               .object();
        request.insert("current_folder", directory.path());
        FileChooser chooser(request);
        QTRY_COMPARE(chooser.rowCount(), 2);
        QVERIFY(rowFor(chooser, "document.txt") >= 0);
        QVERIFY(rowFor(chooser, "photo.png") >= 0);
    }

    void globIgnoresCase()
    {
        QTemporaryDir directory;
        writeFile(directory.filePath("IMG_0001.JPG"));
        writeFile(directory.filePath("notes.txt"));
        auto request = QJsonDocument::fromJson(R"({"mode":"OpenFile",
            "filters":[{"name":"Images", "rules":[{"type":0,"value":"*.jpg"}]}]})")
                               .object();
        request.insert("current_folder", directory.path());
        FileChooser chooser(request);
        QTRY_COMPARE(chooser.rowCount(), 1);
        QVERIFY(rowFor(chooser, "IMG_0001.JPG") >= 0);
    }

    void selectCurrentFolder()
    {
        QTemporaryDir directory;
        writeFile(directory.filePath("hidden-from-chooser.txt"));
        FileChooser chooser(
                {{"mode", "OpenFile"}, {"directory", true}, {"current_folder", directory.path()}});
        QSignalSpy completed(&chooser, &FileChooser::finished);
        chooser.accept();
        QCOMPARE(completed.size(), 1);
        QCOMPARE(
                completed.first().first().toJsonObject().value("uris").toArray().first().toString(),
                QUrl::fromLocalFile(directory.path()).toString());
    }

    void saveRequiresOverwriteConfirmation()
    {
        QTemporaryDir directory;
        const QString existing = directory.filePath("report.txt");
        writeFile(existing);
        FileChooser chooser({{"mode", "SaveFile"}, {"current_file", existing}});
        QCOMPARE(chooser.fileName(), QStringLiteral("report.txt"));
        QCOMPARE(chooser.directory(), directory.path());
        QSignalSpy confirmation(&chooser, &FileChooser::confirmOverwrite);
        QSignalSpy completed(&chooser, &FileChooser::finished);
        chooser.accept(true);
        QCOMPARE(confirmation.size(), 1);
        QVERIFY(completed.isEmpty());
        chooser.accept(true);
        QCOMPARE(completed.size(), 1);
        QFile file(existing);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("example\n"));
    }

    void rejectInvalidNameAndMissingSelection()
    {
        QTemporaryDir directory;
        FileChooser save({{"mode", "SaveFile"},
                          {"current_folder", directory.path()},
                          {"current_name", "../escape"}});
        QSignalSpy completed(&save, &FileChooser::finished);
        save.accept();
        QVERIFY(completed.isEmpty());
        QVERIFY(!save.error().isEmpty());
        FileChooser open({{"mode", "OpenFile"}, {"current_folder", directory.path()}});
        open.accept();
        QVERIFY(!open.error().isEmpty());
    }

    void saveFilesAvoidsCollisions()
    {
        QTemporaryDir directory;
        writeFile(directory.filePath("report.txt"));
        FileChooser chooser({{"mode", "SaveFiles"},
                             {"current_folder", directory.path()},
                             {"files", QJsonArray {"report.txt", "report.txt"}}});
        QSignalSpy completed(&chooser, &FileChooser::finished);
        chooser.accept();
        QCOMPARE(completed.size(), 1);
        const auto uris = completed.first().first().toJsonObject().value("uris").toArray();
        QCOMPARE(uris.size(), 2);
        QVERIFY(uris.at(0) != uris.at(1));
        QVERIFY(uris.first().toString().contains("report%20(1).txt"));
        QVERIFY(!QFileInfo::exists(directory.filePath("report (1).txt")));
    }

    void qmlDialogLoadsAndAccepts()
    {
        QTemporaryDir directory;
        QDir(directory.path()).mkdir("Documents");
        writeFile(directory.filePath("Notes.txt"));
        writeFile(directory.filePath("Project proposal.pdf"));
        FileChooser chooser({{"mode", "SaveFile"},
                             {"title", "Save File"},
                             {"current_folder", directory.path()},
                             {"current_name", "Untitled.txt"}});
        QQmlApplicationEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.setInitialProperties({{"chooser", QVariant::fromValue(&chooser)}});
        engine.load(QUrl("qrc:/chooser/FileChooser.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->show();
        QCOMPARE(window->minimumSize(), window->maximumSize());
        QVERIFY(QMetaObject::invokeMethod(window, "toggleMaximized"));
        QCOMPARE(window->visibility(), QWindow::Windowed);
        QTRY_COMPARE(chooser.rowCount(), 3);
        auto *button = window->findChild<QObject *>("acceptButton");
        QVERIFY(button);
        QCOMPARE(button->property("primary").toBool(), true);
        auto *cancel = window->findChild<QObject *>("cancelButton");
        QVERIFY(cancel);
        QCOMPARE(cancel->property("primary").toBool(), false);
        const QString screenshot = qEnvironmentVariable("CHOOSER_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QTest::qWait(300);
            QVERIFY(window->grabWindow().save(screenshot));
        }
        QSignalSpy completed(&chooser, &FileChooser::finished);
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QCOMPARE(completed.size(), 1);
    }

    void saveDialogAcceptsEnter()
    {
        QTemporaryDir directory;
        FileChooser chooser({{"mode", "SaveFile"},
                             {"current_folder", directory.path()},
                             {"current_name", "photo.jpeg"}});
        QQmlApplicationEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.setInitialProperties({{"chooser", QVariant::fromValue(&chooser)}});
        engine.load(QUrl("qrc:/chooser/FileChooser.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->show();
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        auto *field = window->findChild<QQuickItem *>("fileNameField");
        QVERIFY(field);
        QTRY_VERIFY(field->hasActiveFocus());
        QCOMPARE(field->property("selectedText").toString(), QStringLiteral("photo"));

        QSignalSpy completed(&chooser, &FileChooser::finished);
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(completed.size(), 1);
        const auto result = completed.first().first().toJsonObject();
        QCOMPARE(result.value("response").toInt(), 0);
        QCOMPARE(result.value("uris").toArray().first().toString(),
                 QUrl::fromLocalFile(directory.filePath("photo.jpeg")).toString());
    }

    void pathEditorClosesOnOutsidePress()
    {
        QTemporaryDir directory;
        writeFile(directory.filePath("Notes.txt"));
        FileChooser chooser({{"mode", "OpenFile"}, {"current_folder", directory.path()}});
        QQmlApplicationEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.setInitialProperties({{"chooser", QVariant::fromValue(&chooser)}});
        engine.load(QUrl("qrc:/chooser/FileChooser.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->show();
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        QTRY_COMPARE(chooser.rowCount(), 1);

        auto *editor = window->findChild<QQuickItem *>("locationField");
        QVERIFY(editor);
        QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
        QTRY_VERIFY(editor->hasActiveFocus());

        // Whatever watches for the outside press must not take the cursor from the editor.
        const QPoint inEditor = editor->mapToScene(QPointF(editor->width() / 2, editor->height() / 2)).toPoint();
        QTest::mouseMove(window, inEditor);
        QTRY_COMPARE(window->cursor().shape(), Qt::IBeamCursor);

        // Nor swallow the press: it closes the editor and still selects the row under it.
        auto *list = window->findChild<QQuickItem *>("fileList");
        QVERIFY(list);
        QQuickItem *row = nullptr;
        QVERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0)));
        QVERIFY(row);
        const QPoint inRow = row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, {}, inRow);
        QTRY_VERIFY(!editor->isVisible());
        QVERIFY(chooser.isSelected(0));
    }

    void pathEditorCompletesFolders()
    {
        QTemporaryDir directory;
        QDir root(directory.path());
        QVERIFY(root.mkpath("Documents/Work"));
        QVERIFY(root.mkdir("Downloads"));
        QVERIFY(root.mkdir(".Drafts"));
        writeFile(directory.filePath("Diary.txt"));
        FileChooser chooser({{"mode", "OpenFile"}, {"current_folder", directory.path()}});
        QQmlApplicationEngine engine;
        CutefishFM::registerImageProviders(&engine);
        engine.setInitialProperties({{"chooser", QVariant::fromValue(&chooser)}});
        engine.load(QUrl("qrc:/chooser/FileChooser.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->show();
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));

        auto *editor = window->findChild<QQuickItem *>("locationField");
        QVERIFY(editor);
        auto *completion = window->findChild<QObject *>("pathCompletion");
        QVERIFY(completion);
        QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
        QTRY_VERIFY(editor->hasActiveFocus());
        // Opening the editor lists nothing until something is typed.
        QVERIFY(!completion->property("visible").toBool());

        // Only folders, and hidden ones only once a dot is typed.
        for (const QChar c : directory.path() + "/D")
            QTest::keyClick(window, c.toLatin1());
        QTRY_VERIFY(completion->property("visible").toBool());
        QCOMPARE(completion->property("count").toInt(), 2);

        // Tab extends to what both share, then to the one folder left.
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(editor->property("text").toString(), directory.path() + "/Do");
        QVERIFY(editor->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_C);
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(editor->property("text").toString(), directory.path() + "/Documents/");
        QCOMPARE(completion->property("count").toInt(), 1);

        // Escape hides the list but keeps the editor; Down brings it back.
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY(!completion->property("visible").toBool());
        QVERIFY(editor->isVisible());
        QTest::keyClick(window, Qt::Key_Down);
        QVERIFY(completion->property("visible").toBool());

        QTest::keyClick(window, Qt::Key_Down);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(chooser.directory(), directory.filePath("Documents/Work"));
        QTRY_VERIFY(!editor->isVisible());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir environment;
    qputenv("XDG_CONFIG_HOME", environment.filePath("config").toUtf8());
    qputenv("XDG_CACHE_HOME", environment.filePath("cache").toUtf8());
    qputenv("XDG_DATA_HOME", environment.filePath("data").toUtf8());
    QApplication app(argc, argv);
    QIcon::setThemeName(QStringLiteral("Cutefish"));
    CutefishFM::registerQmlTypes();
    CutefishFM::initResources();
    FileChooserTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_filechooser.moc"
