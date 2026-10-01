// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pathhistory.h"
#include <QUrl>

PathHistory::PathHistory(QObject *parent)
    : QObject(parent)
{

}

void PathHistory::append(const QUrl &path)
{
    // A new destination ends the forward branch, as in a browser; the back
    // list is bounded so a long-lived tab does not keep every step.
    m_postHistory.clear();
    m_prevHistory.append(path);
    while (m_prevHistory.size() > kMaxEntries)
        m_prevHistory.removeFirst();
}

void PathHistory::replaceCurrent(const QUrl &path)
{
    if (m_prevHistory.isEmpty()) {
        m_prevHistory.append(path);
        return;
    }

    m_prevHistory.last() = path;

    if (m_prevHistory.size() > 1 && m_prevHistory[m_prevHistory.size() - 2] == path)
        m_prevHistory.removeLast();
}

QUrl PathHistory::first()
{
    return m_prevHistory.first();
}

QUrl PathHistory::last()
{
    return m_prevHistory.last();
}

QUrl PathHistory::at(int i)
{
    return m_prevHistory.at(i);
}

int PathHistory::count()
{
    return m_prevHistory.count();
}

bool PathHistory::isEmpty()
{
    return m_prevHistory.isEmpty();
}

bool PathHistory::canGoBack() const
{
    return m_prevHistory.length() >= 2;
}

bool PathHistory::canGoForward() const
{
    return !m_postHistory.isEmpty();
}

QUrl PathHistory::posteriorPath()
{
    if (m_postHistory.isEmpty())
        return QUrl();

    return m_postHistory.takeLast();
}

QList<QUrl> PathHistory::backward() const
{
    return m_prevHistory.isEmpty() ? QList<QUrl>() : m_prevHistory.first(m_prevHistory.size() - 1);
}

QList<QUrl> PathHistory::forward() const
{
    return m_postHistory;
}

void PathHistory::restore(const QList<QUrl> &backward, const QList<QUrl> &forward)
{
    const QUrl current = m_prevHistory.isEmpty() ? QUrl() : m_prevHistory.last();
    m_prevHistory = backward;
    if (!current.isEmpty())
        m_prevHistory.append(current);
    m_postHistory = forward;
}

QUrl PathHistory::previousPath()
{
    if (m_prevHistory.isEmpty())
        return QUrl();

    if (m_prevHistory.length() < 2)
        return m_prevHistory.at(0);

    m_postHistory.append(m_prevHistory.takeLast());
    return m_prevHistory.takeLast();
}
