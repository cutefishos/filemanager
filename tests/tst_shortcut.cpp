#include "helper/shortcut.h"
#include "helper/shortcutconvention.h"

#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>

class TestShortCut : public QObject
{
    Q_OBJECT

private slots:
    void commandKeysWorkOnlyUnderTheCommandConvention();
};

void TestShortCut::commandKeysWorkOnlyUnderTheCommandConvention()
{
    QObject view;
    ShortCut shortcut;
    shortcut.install(&view);
    QSignalSpy deleted(&shortcut, &ShortCut::deleteFile);
    QSignalSpy backspace(&shortcut, &ShortCut::backspace);
    QSignalSpy up(&shortcut, &ShortCut::up);
    QSignalSpy open(&shortcut, &ShortCut::open);
    QSignalSpy back(&shortcut, &ShortCut::back);
    QSignalSpy forward(&shortcut, &ShortCut::forward);
    QSignalSpy properties(&shortcut, &ShortCut::properties);
    QSignalSpy hidden(&shortcut, &ShortCut::showHidden);

    const auto press = [&view](int key, Qt::KeyboardModifiers modifiers) {
        QKeyEvent event(QEvent::KeyPress, key, modifiers);
        QCoreApplication::sendEvent(&view, &event);
    };

    // Under the Ctrl convention Ctrl+Backspace is still Backspace.
    ShortcutConvention::self()->setCommandKeys(false);
    press(Qt::Key_Backspace, Qt::ControlModifier);
    QCOMPARE(deleted.count(), 0);
    QCOMPARE(backspace.count(), 1);

    // Under Command these arrive as Ctrl.
    ShortcutConvention::self()->setCommandKeys(true);
    press(Qt::Key_Backspace, Qt::ControlModifier);
    press(Qt::Key_Up, Qt::ControlModifier);
    press(Qt::Key_Down, Qt::ControlModifier);
    press(Qt::Key_BracketLeft, Qt::ControlModifier);
    press(Qt::Key_BracketRight, Qt::ControlModifier);
    press(Qt::Key_I, Qt::ControlModifier);
    press(Qt::Key_Greater, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(deleted.count(), 1);
    QCOMPARE(backspace.count(), 1);
    QCOMPARE(up.count(), 1);
    QCOMPARE(open.count(), 1);
    QCOMPARE(back.count(), 1);
    QCOMPARE(forward.count(), 1);
    QCOMPARE(properties.count(), 1);
    QCOMPARE(hidden.count(), 1);

    // Plain Backspace still goes up a folder.
    press(Qt::Key_Backspace, Qt::NoModifier);
    QCOMPARE(backspace.count(), 2);
}

QTEST_MAIN(TestShortCut)

#include "tst_shortcut.moc"
