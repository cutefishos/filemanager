// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardsearchmanager.h"

KeyboardSearchManager *KEYBORDSRARCH_MANAGER_SELF = nullptr;

KeyboardSearchManager *KeyboardSearchManager::self()
{
    if (!KEYBORDSRARCH_MANAGER_SELF)
        KEYBORDSRARCH_MANAGER_SELF = new KeyboardSearchManager;

    return KEYBORDSRARCH_MANAGER_SELF;
}

KeyboardSearchManager::KeyboardSearchManager(QObject *parent)
    : QObject(parent)
    , m_timeout(500)
{
    // m_timer.setInterval(m_timeout);
    // connect(&m_timer, &QTimer::timeout, this, [=] {
    //     m_searchText.clear();
    // });
}

void KeyboardSearchManager::addKeys(const QString &keys)
{
    if (!keys.isEmpty()) {
        // m_timer.stop();
        // m_searchText.append(keys);

        // const QChar firstKey = m_searchText.length() > 0 ? m_searchText.at(0) : QChar();
        // const bool sameKey = m_searchText.length() > 1 && m_searchText.count(firstKey) == m_searchText.length();

        // emit searchTextChanged(sameKey ? firstKey : m_searchText, false);
        emit searchTextChanged(keys, false);

        // m_timer.start();
    }
}
