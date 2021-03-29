#include "pathhistory.h"
#include <QUrl>

PathHistory::PathHistory(QObject *parent)
    : QObject(parent)
{

}

void PathHistory::append(const QUrl &path)
{
    m_prevHistory.append(path);
}

QUrl PathHistory::posteriorPath()
{
    if (m_postHistory.isEmpty())
        return QUrl();

    return m_postHistory.takeLast();
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

