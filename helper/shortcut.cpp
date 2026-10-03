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

#include "shortcut.h"
#include "shortcutconvention.h"

#include <QKeyEvent>

ShortCut::ShortCut(QObject *parent)
    : QObject(parent)
{
    // The convention is read asynchronously; starting it on the first key press
    // leaves that press under the Ctrl convention.
    ShortcutConvention::self();
}

void ShortCut::install(QObject *target)
{
    if (target)
        target->installEventFilter(this);
}

bool ShortCut::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);

        if (m_previewEnabled && keyEvent->key() == Qt::Key_Space
            && keyEvent->modifiers() == Qt::NoModifier) {
            if (!keyEvent->isAutoRepeat())
                emit preview();
            return true;
        }

        if (ShortcutConvention::self()->commandKeys() && commandKey(keyEvent))
            return true;

        // Alt+Up, Alt+Left, Alt+Right and Alt+Enter, as in other file managers.
        if (keyEvent->modifiers() == Qt::AltModifier) {
            switch (keyEvent->key()) {
            case Qt::Key_Up:
                emit up();
                return true;
            case Qt::Key_Left:
                emit back();
                return true;
            case Qt::Key_Right:
                emit forward();
                return true;
            case Qt::Key_Enter:
            case Qt::Key_Return:
                emit properties();
                return true;
            default:
                break;
            }
        }

        if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
            emit open();
        } else if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers() & Qt::ControlModifier) {
            emit copy();
        } else if (keyEvent->key() == Qt::Key_X && keyEvent->modifiers() & Qt::ControlModifier) {
            emit cut();
        } else if (keyEvent->key() == Qt::Key_W && keyEvent->modifiers() & Qt::ControlModifier) {
            emit close();
        } else if (keyEvent->key() == Qt::Key_V && keyEvent->modifiers() & Qt::ControlModifier) {
            emit paste();
        } else if (keyEvent->key() == Qt::Key_F2) {
            emit rename();
        } else if (keyEvent->key() == Qt::Key_L && keyEvent->modifiers() & Qt::ControlModifier) {
            emit openPathEditor();
        } else if (keyEvent->key() == Qt::Key_A && keyEvent->modifiers() & Qt::ControlModifier) {
            emit selectAll();
        } else if (keyEvent->key() == Qt::Key_Backspace) {
            emit backspace();
        } else if (keyEvent->key() == Qt::Key_Delete) {
            emit deleteFile();
        } else if (keyEvent->key() == Qt::Key_F5) {
            emit refresh();
        } else if (keyEvent->key() == Qt::Key_H && keyEvent->modifiers() & Qt::ControlModifier) {
            emit showHidden();
        } else if (keyEvent->key() == Qt::Key_Z && keyEvent->modifiers() & Qt::ControlModifier) {
            if (keyEvent->modifiers() & Qt::ShiftModifier)
                emit redo();
            else
                emit undo();
        } else if (keyEvent->key() >= Qt::Key_A && keyEvent->key() <= Qt::Key_Z) {
            emit keyPressed(keyEvent->text());
            keyEvent->ignore();
        }
    }

    return QObject::eventFilter(obj, e);
}

// Command reaches us as Ctrl; outside text fields the compositor leaves these
// keys to the file view.
bool ShortCut::commandKey(const QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;

    if (modifiers == Qt::ControlModifier) {
        switch (event->key()) {
        case Qt::Key_Backspace:
            emit deleteFile();
            return true;
        case Qt::Key_Up:
            emit up();
            return true;
        case Qt::Key_Down:
            emit open();
            return true;
        case Qt::Key_BracketLeft:
            emit back();
            return true;
        case Qt::Key_BracketRight:
            emit forward();
            return true;
        case Qt::Key_I:
            emit properties();
            return true;
        default:
            break;
        }
    }

    // Command+Shift+.; Shift makes the key ">" on most layouts.
    if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)
        && (event->key() == Qt::Key_Period || event->key() == Qt::Key_Greater)) {
        emit showHidden();
        return true;
    }

    return false;
}
