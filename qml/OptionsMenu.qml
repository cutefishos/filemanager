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

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import FishUI 1.0 as FishUI

FishUI.DesktopMenu {
    id: control

    FishUI.MenuItem {
        text: qsTr("Icons")
        reservesCheckColumn: true
        checked: settings.viewMethod === 1
        onTriggered: settings.viewMethod = 1
    }

    FishUI.MenuItem {
        text: qsTr("List")
        reservesCheckColumn: true
        checked: settings.viewMethod === 0
        onTriggered: settings.viewMethod = 0
    }

    FishUI.MenuSeparator {}

    FishUI.MenuItem {
        text: qsTr("Name")
        reservesCheckColumn: true
        checked: settings.sortMode === 0
        onTriggered: settings.sortMode = 0
    }

    FishUI.MenuItem {
        text: qsTr("Date")
        reservesCheckColumn: true
        checked: settings.sortMode === 2
        onTriggered: settings.sortMode = 2
    }

    FishUI.MenuItem {
        text: qsTr("Type")
        reservesCheckColumn: true
        checked: settings.sortMode === 6
        onTriggered: settings.sortMode = 6
    }

    FishUI.MenuItem {
        text: qsTr("Size")
        reservesCheckColumn: true
        checked: settings.sortMode === 1
        onTriggered: settings.sortMode = 1
    }
}
