// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pathhistory.h"

PathHistory::PathHistory(QObject *parent)
    : QObject(parent)
{

}

void PathHistory::append(const QUrl &path)
{
    // A new destination ends the forward branch, as in a browser; the back
    // list is bounded so a long-lived tab does not keep every step.
    m_postHistory.clear();
    m_prevHistory.append({path});
    while (m_prevHistory.size() > kMaxEntries)
        m_prevHistory.removeFirst();
}

void PathHistory::replaceCurrent(const QUrl &path)
{
    if (m_prevHistory.isEmpty()) {
        m_prevHistory.append({path});
        return;
    }

    m_prevHistory.last().url = path;

    if (m_prevHistory.size() > 1 && m_prevHistory[m_prevHistory.size() - 2].url == path)
        m_prevHistory.removeLast();
}

QUrl PathHistory::first()
{
    return m_prevHistory.first().url;
}

QUrl PathHistory::last()
{
    return m_prevHistory.last().url;
}

QUrl PathHistory::at(int i)
{
    return m_prevHistory.at(i).url;
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

    m_prevHistory.append(m_postHistory.takeLast());
    return m_prevHistory.last().url;
}

QUrl PathHistory::previousPath()
{
    if (!canGoBack())
        return QUrl();

    m_postHistory.append(m_prevHistory.takeLast());
    return m_prevHistory.last().url;
}

qreal PathHistory::currentPosition() const
{
    return m_prevHistory.isEmpty() ? qQNaN() : m_prevHistory.last().position;
}

void PathHistory::setCurrentPosition(qreal position)
{
    if (!m_prevHistory.isEmpty())
        m_prevHistory.last().position = position;
}

QList<QUrl> PathHistory::currentSelection() const
{
    return m_prevHistory.isEmpty() ? QList<QUrl>() : m_prevHistory.last().selection;
}

void PathHistory::setCurrentSelection(const QList<QUrl> &selection)
{
    if (!m_prevHistory.isEmpty())
        m_prevHistory.last().selection = selection;
}

QList<QUrl> PathHistory::backward() const
{
    return m_prevHistory.isEmpty() ? QList<QUrl>() : urls(m_prevHistory.first(m_prevHistory.size() - 1));
}

QList<QUrl> PathHistory::forward() const
{
    return urls(m_postHistory);
}

void PathHistory::restore(const QList<QUrl> &backward, const QList<QUrl> &forward)
{
    const QUrl current = m_prevHistory.isEmpty() ? QUrl() : m_prevHistory.last().url;
    m_prevHistory = entries(backward);
    if (!current.isEmpty())
        m_prevHistory.append({current});
    m_postHistory = entries(forward);
}

QList<QUrl> PathHistory::urls(const QVector<Entry> &entries)
{
    QList<QUrl> list;
    list.reserve(entries.size());
    for (const Entry &entry : entries)
        list.append(entry.url);
    return list;
}

QVector<PathHistory::Entry> PathHistory::entries(const QList<QUrl> &urls)
{
    QVector<Entry> list;
    list.reserve(urls.size());
    for (const QUrl &url : urls)
        list.append({url});
    return list;
}
