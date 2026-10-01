// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATHHISTORY_H
#define PATHHISTORY_H

#include <QObject>
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

    QUrl posteriorPath();
    QUrl previousPath();

    // The folders before and after the current one, oldest first.
    QList<QUrl> backward() const;
    QList<QUrl> forward() const;
    // Puts them around the current folder, as a tab carried to another window had them.
    void restore(const QList<QUrl> &backward, const QList<QUrl> &forward);

private:
    static constexpr int kMaxEntries = 200;
    QVector<QUrl> m_prevHistory;
    QVector<QUrl> m_postHistory;
};

#endif // PATHHISTORY_H
