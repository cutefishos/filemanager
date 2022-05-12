/*
    SPDX-FileCopyrightText: 2010 BetterInbox <contact@betterinbox.com>
    SPDX-FileContributor: Gregory Schlomoff <greg@betterinbox.com>

    SPDX-License-Identifier: MIT
*/

#include "declarativedroparea.h"
#include "declarativedragdropevent.h"

DeclarativeDropArea::DeclarativeDropArea(QQuickItem *parent)
    : QQuickItem(parent)
    , m_enabled(true)
    , m_preventStealing(false)
    , m_temporaryInhibition(false)
    , m_containsDrag(false)
{
    setFlag(ItemAcceptsDrops, m_enabled);
}

void DeclarativeDropArea::temporaryInhibitParent(bool inhibit)
{
    QQuickItem *candidate = parentItem();

    while (candidate) {
        if (DeclarativeDropArea *da = qobject_cast<DeclarativeDropArea *>(candidate)) {
            da->m_temporaryInhibition = inhibit;
            if (inhibit) {
                Q_EMIT da->dragLeaveEvent(nullptr);
            }
        }
        candidate = candidate->parentItem();
    }
}

void DeclarativeDropArea::dragEnterEvent(QDragEnterEvent *event)
{
    if (!m_enabled || m_temporaryInhibition) {
        return;
    }

    DeclarativeDragDropEvent dde(event, this);
    event->accept();

    Q_EMIT dragEnter(&dde);

    if (!event->isAccepted()) {
        return;
    }

    if (m_preventStealing) {
        temporaryInhibitParent(true);
    }

    m_oldDragMovePos = event->pos();
    setContainsDrag(true);
}

void DeclarativeDropArea::dragLeaveEvent(QDragLeaveEvent *event)
{
    // do it anyways, in the unlikely case m_preventStealing
    // was changed while drag
    temporaryInhibitParent(false);

    m_oldDragMovePos = QPoint(-1, -1);
    DeclarativeDragDropEvent dde(event, this);
    Q_EMIT dragLeave(&dde);
    setContainsDrag(false);
}

void DeclarativeDropArea::dragMoveEvent(QDragMoveEvent *event)
{
    if (!m_enabled || m_temporaryInhibition) {
        event->ignore();
        return;
    }
    event->accept();
    // if the position we export didn't change, don't generate the move event
    if (event->pos() == m_oldDragMovePos) {
        return;
    }

    m_oldDragMovePos = event->pos();
    DeclarativeDragDropEvent dde(event, this);
    Q_EMIT dragMove(&dde);
}

void DeclarativeDropArea::dropEvent(QDropEvent *event)
{
    // do it anyways, in the unlikely case m_preventStealing
    // was changed while drag, do it after a loop,
    // so the parent dropevent doesn't get delivered
    metaObject()->invokeMethod(this, "temporaryInhibitParent", Qt::QueuedConnection, Q_ARG(bool, false));

    m_oldDragMovePos = QPoint(-1, -1);

    if (!m_enabled || m_temporaryInhibition) {
        return;
    }

    DeclarativeDragDropEvent dde(event, this);
    Q_EMIT drop(&dde);
    setContainsDrag(false);
}

bool DeclarativeDropArea::isEnabled() const
{
    return m_enabled;
}

void DeclarativeDropArea::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }

    m_enabled = enabled;
    setFlag(ItemAcceptsDrops, m_enabled);
    Q_EMIT enabledChanged();
}

bool DeclarativeDropArea::preventStealing() const
{
    return m_preventStealing;
}

void DeclarativeDropArea::setPreventStealing(bool prevent)
{
    if (prevent == m_preventStealing) {
        return;
    }

    m_preventStealing = prevent;
    Q_EMIT preventStealingChanged();
}

void DeclarativeDropArea::setContainsDrag(bool dragging)
{
    if (m_containsDrag != dragging) {
        m_containsDrag = dragging;
        Q_EMIT containsDragChanged(m_containsDrag);
    }
}

bool DeclarativeDropArea::containsDrag() const
{
    return m_containsDrag;
}
