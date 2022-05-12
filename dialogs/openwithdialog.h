/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
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

#ifndef OPENWITHDIALOG_H
#define OPENWITHDIALOG_H

#include <QQuickView>

class OpenWithDialog : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QString url READ url CONSTANT)

public:
    explicit OpenWithDialog(const QUrl &url, QQuickView *parent = nullptr);

    QString url() const;

private:
    QString m_url;
};

#endif // OPENWITHDIALOG_H
