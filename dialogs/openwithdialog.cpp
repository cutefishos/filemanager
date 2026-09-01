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

#include "openwithdialog.h"
#include "../mimetype/mimeappmanager.h"
#include "../helper/filelauncher.h"

#include <QQmlEngine>
#include <QQmlContext>

OpenWithDialog::OpenWithDialog(const QUrl &url, QObject *parent)
    : Window(parent)
    , m_url(url.toLocalFile())
{
    rootContext()->setContextProperty("main", this);
    rootContext()->setContextProperty("mimeAppManager", MimeAppManager::self());
    rootContext()->setContextProperty("launcher", FileLauncher::self());

    load(QUrl("qrc:/qml/Dialogs/OpenWithDialog.qml"));
}

QString OpenWithDialog::url() const
{
    return m_url;
}
