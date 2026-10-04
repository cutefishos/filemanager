// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATHHISTORY_H
#define PATHHISTORY_H

#include <QObject>
#include <QUrl>
#include <QVector>

class PathHistory : public QObject
{
    Q_OBJECT

public:
    explicit PathHistory(QObject *parent = nullptr);

    void append(const QUrl &path);
    void replaceCurrent(const QUrl &path);

    QUrl first();
    QUrl last();

    QUrl at(int i);
    int count();

    bool isEmpty();

    bool canGoBack() const;
    bool canGoForward() const;

    // Step through the history and return the folder now current, or an
    // empty URL when there is nothing that way.
    QUrl posteriorPath();
    QUrl previousPath();

    // Where the view was scrolled in the current folder; NaN until it is left.
    qreal currentPosition() const;
    void setCurrentPosition(qreal position);

    // The folders before and after the current one, oldest first.
    QList<QUrl> backward() const;
    QList<QUrl> forward() const;
    // Puts them around the current folder, as a tab carried to another window had them.
    void restore(const QList<QUrl> &backward, const QList<QUrl> &forward);

private:
    struct Entry {
        QUrl url;
        qreal position = qQNaN();
    };

    static QList<QUrl> urls(const QVector<Entry> &entries);
    static QVector<Entry> entries(const QList<QUrl> &urls);

    static constexpr int kMaxEntries = 200;
    QVector<Entry> m_prevHistory;
    QVector<Entry> m_postHistory;
};

#endif // PATHHISTORY_H
