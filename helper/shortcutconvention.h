#pragma once

#include <QObject>

// The compositor's shortcut convention, com.cutefish.Keyboard1.shortcutConvention.
// Under "command" the key next to the space bar reaches applications as Ctrl
// and leaves the file view keys (Command+Backspace, Command+Up) to us.
class ShortcutConvention : public QObject
{
    Q_OBJECT

public:
    static ShortcutConvention *self();

    bool commandKeys() const { return m_commandKeys; }
    void setCommandKeys(bool commandKeys);

private Q_SLOTS:
    void refresh();

private:
    explicit ShortcutConvention(QObject *parent = nullptr);

    bool m_commandKeys = false;
};
