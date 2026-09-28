/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     revenmartin <revenmartin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
