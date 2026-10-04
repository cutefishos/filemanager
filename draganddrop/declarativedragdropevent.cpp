/*
    SPDX-FileCopyrightText: 2010 BetterInbox <contact@betterinbox.com>
    SPDX-FileContributor: Gregory Schlomoff <greg@betterinbox.com>
    SPDX-FileCopyrightText: 2013 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: MIT
*/

#include "declarativedragdropevent.h"

DeclarativeDragDropEvent::DeclarativeDragDropEvent(QDropEvent *e, DeclarativeDropArea *parent)
    : QObject(parent)
    , m_x(e->pos().x())
    , m_y(e->pos().y())
    , m_buttons(e->mouseButtons())
    , m_modifiers(e->keyboardModifiers())
    , m_event(e)
{
}

DeclarativeDragDropEvent::DeclarativeDragDropEvent(QDragLeaveEvent *e, DeclarativeDropArea *parent)
    : QObject(parent)
    , m_x(0)
    , m_y(0)
    , m_buttons(Qt::NoButton)
    , m_modifiers(Qt::NoModifier)
    , m_event(nullptr)
{
    Q_UNUSED(e);
}

void DeclarativeDragDropEvent::accept(int action)
{
    m_event->setDropAction(static_cast<Qt::DropAction>(action));
    m_event->accept();
}

void DeclarativeDragDropEvent::ignore()
{
    m_event->ignore();
}

QMimeData *DeclarativeDragDropEvent::mimeData() const
{
    // QML only reads it, but Q_PROPERTY cannot carry a const pointer.
    return m_event ? const_cast<QMimeData *>(m_event->mimeData()) : nullptr;
}
