// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-FileCopyrightText: Ken <https://stackoverflow.com/users/1568857/ken>
// SPDX-FileCopyrightText: 2016 Leslie Zhai <xiangzhai83@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHORTCUT_H
#define SHORTCUT_H

#include <QObject>

class QKeyEvent;

class ShortCut : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool previewEnabled MEMBER m_previewEnabled)

public:
    explicit ShortCut(QObject *parent = nullptr);

    Q_INVOKABLE void install(QObject *target = nullptr);

signals:
    void preview();
    void open();
    void copy();
    void cut();
    void paste();
    void rename();
    void refresh();
    void openPathEditor();
    void selectAll();
    void backspace();
    void up();
    void back();
    void forward();
    void properties();
    void deleteFile();
    void showHidden();
    void keyPressed(const QString &text);
    void close();
    void undo();
    void redo();

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    bool commandKey(const QKeyEvent *event);

    bool m_previewEnabled = false;
};

#endif // SHORTCUT_H
