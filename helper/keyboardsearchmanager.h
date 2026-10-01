// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later


#ifndef KEYBOARDSEARCHMANAGER_H
#define KEYBOARDSEARCHMANAGER_H

#include <QObject>
#include <QTimer>

class KeyboardSearchManager : public QObject
{
    Q_OBJECT

public:
    static KeyboardSearchManager *self();
    explicit KeyboardSearchManager(QObject *parent = nullptr);

    void addKeys(const QString &keys);

signals:
    void searchTextChanged(const QString &string, bool searchFromNextItem);

private:
    QString m_searchText;
    qint64 m_timeout;
    // QTimer m_timer;
};

#endif // KEYBOARDSEARCHMANAGER_H
