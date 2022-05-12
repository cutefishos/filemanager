/***************************************************************************
 *   Copyright 2021 Reion Wong <reion@cutefishos.com>                      *
 *   Copyright Ken <https://stackoverflow.com/users/1568857/ken>           *
 *   Copyright 2016 Leslie Zhai <xiangzhai83@gmail.com>                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#pragma once

#include <QObject>

class ShortCut : public QObject
{
    Q_OBJECT

    public:
        explicit ShortCut(QObject *parent = nullptr);

        Q_INVOKABLE void install(QObject *target = nullptr);

    signals:
        void open();
        void copy();
        void cut();
        void paste();
        void rename();
        void refresh();
        void openPathEditor();
        void selectAll();
        void backspace();
        void deleteFile();
        void showHidden();
        void keyPressed(const QString &text);
        void close();
        void undo();

    protected:
        bool eventFilter(QObject *obj, QEvent *e) override;

    private:
        QObject *m_object;

        // This is a structure composed of :
        // - a key (the shortcut)
        // - an action (the function pointer)
        typedef struct {
            Qt::Key key;
            std::function<void(void)> action;
        } fun_shortcut;

        // This array contains structures where keys are associated
        // with an action. You only have to loop on this array, compare
        // its key with the one you received, and execute its action
        const fun_shortcut Normal_Shortcuts[14] = {
            { Qt::Key_Y, [&]() { return copy(); } },
            { Qt::Key_P, [&]() { return paste(); } },
            { Qt::Key_R, [&]() { return rename(); } },
            { Qt::Key_Return, [&]() { return open(); } },
            { Qt::Key_Enter, [&]() { return open(); } },
            { Qt::Key_Percent, [&]() { return showHidden(); } },
            { Qt::Key_U, [&]() { return undo(); } },
            { Qt::Key_D, [&]() { return cut(); } },
            { Qt::Key_X, [&]() { return deleteFile(); } },
            { Qt::Key_Q, [&]() { return close(); } },
            { Qt::Key_A, [&]() { return refresh(); } },
            { Qt::Key_Colon, [&]() { return openPathEditor(); } },
            { Qt::Key_V, [&]() { return selectAll(); } },
            { Qt::Key_Backspace, [&]() { return backspace(); } }
        };
};
