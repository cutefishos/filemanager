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
#include "keyboardsearchmanager.h"

#include <QKeyEvent>

ShortCut::ShortCut(QObject *parent)
    : QObject(parent)
    , m_object(parent)
{
}

void ShortCut::install(QObject *target)
{
    // No need to remove, because memory space is automatically freed in qml.
    // if (m_object) {
    //     m_object->removeEventFilter(this);
    // }

    if (target) {
        target->installEventFilter(this);
        // m_object = target;
    }
}

bool ShortCut::eventFilter(QObject *obj, QEvent *e)
{
    if (e->type() != QEvent::KeyPress) {
        return QObject::eventFilter(obj, e);
    }

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
    int key = keyEvent->key();

    for (const fun_shortcut &shortcut: Normal_Shortcuts) {
        if (key == shortcut.key) {
            shortcut.action();
        }
    }

    return QObject::eventFilter(obj, e);
}
