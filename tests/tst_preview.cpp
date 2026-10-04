// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "helper/previewcontroller.h"

#include <QFile>
#include <QStringEncoder>
#include <QTemporaryDir>
#include <QtTest>

class PreviewTest : public QObject
{
    Q_OBJECT

private:
    QString write(const QString &name, const QByteArray &contents)
    {
        QFile file(m_dir.filePath(name));
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(contents);
        return file.fileName();
    }

    QVariantMap describe(const QString &path)
    {
        PreviewController controller;
        controller.present({QVariantMap{{"url", QUrl::fromLocalFile(path)}}}, 0, false);
        if (!QTest::qWaitFor([&] { return !controller.loading(); }))
            return {};
        return controller.file();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void textIsDecoded_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<QString>("text");

        const QString sample = QStringLiteral("第一行 first\n\tsecond\n");
        QStringEncoder utf16(QStringEncoder::Utf16LE, QStringEncoder::Flag::WriteBom);
        QTest::newRow("utf-8") << "notes.txt" << sample.toUtf8() << sample;
        QTest::newRow("crlf") << "notes.txt" << sample.toUtf8().replace("\n", "\r\n") << sample;
        QTest::newRow("utf-16 with bom") << "notes.txt" << QByteArray(utf16(sample)) << sample;
        QTest::newRow("source code") << "main.cpp" << QByteArray("int main() {}\n")
                                     << "int main() {}\n";
        QTest::newRow("no extension") << "README" << QByteArray("Plain words.\n")
                                      << "Plain words.\n";
    }

    void textIsDecoded()
    {
        QFETCH(QString, name);
        QFETCH(QByteArray, contents);
        QFETCH(QString, text);

        const QVariantMap file = describe(write(name, contents));
        QCOMPARE(file.value("kind").toString(), QStringLiteral("text"));
        QCOMPARE(file.value("text").toString(), text);
        QCOMPARE(file.value("truncated").toBool(), false);
    }

    void binaryContentIsNotText()
    {
        const QVariantMap file = describe(write("data.txt", QByteArray("abc\0def\n", 8)));
        QCOMPARE(file.value("kind").toString(), QStringLiteral("generic"));
        QVERIFY(!file.contains("text"));
    }

    void longTextEndsOnAWholeLine()
    {
        const QByteArray line = QByteArray(99, 'x') + '\n';
        const QVariantMap file = describe(write("long.txt", line.repeated(4000)));
        QCOMPARE(file.value("kind").toString(), QStringLiteral("text"));
        QVERIFY(file.value("truncated").toBool());
        const QString text = file.value("text").toString();
        QVERIFY(text.size() <= 256 * 1024);
        QVERIFY(text.size() > 250 * 1024);
        QVERIFY(text.endsWith(QString(line)));
    }

    void longParagraphsAreSplit_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<bool>("atSpaces");

        QTest::newRow("no spaces") << QString(10000, QLatin1Char('x')) << false;
        QTest::newRow("words") << QStringLiteral("word ").repeated(2000) << true;
        QTest::newRow("surrogate pairs") << QStringLiteral("😀").repeated(5000) << false;
    }

    void longParagraphsAreSplit()
    {
        QFETCH(QString, line);
        QFETCH(bool, atSpaces);

        const QString original = QStringLiteral("short\n") + line + QStringLiteral("\nend\n");
        const QVariantMap file = describe(write("long-line.txt", original.toUtf8()));
        const QString text = file.value("text").toString();
        const QVariantList breaks = file.value("breaks").toList();
        QVERIFY(breaks.size() >= 2);

        QString joined;
        qsizetype position = 0;
        for (const QVariant &value : breaks) {
            const qsizetype at = value.toLongLong();
            QCOMPARE(text.at(at), QLatin1Char('\n'));
            QVERIFY(!text.at(at - 1).isHighSurrogate());
            if (atSpaces)
                QCOMPARE(text.at(at - 1), QLatin1Char(' '));
            joined += text.sliced(position, at - position);
            position = at + 1;
        }
        joined += text.sliced(position);
        QCOMPARE(joined, original);

        for (const QString &paragraph : text.split(QLatin1Char('\n')))
            QVERIFY(paragraph.size() <= 4000);
    }

    void shortParagraphsAreKept()
    {
        const QVariantMap file = describe(write("short.txt", "one\ntwo\n"));
        QVERIFY(file.value("breaks").toList().isEmpty());
    }

private:
    QTemporaryDir m_dir;
};

QTEST_MAIN(PreviewTest)
#include "tst_preview.moc"
