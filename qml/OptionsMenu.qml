// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import FishUI 1.0 as FishUI

FishUI.DesktopMenu {
    id: control

    pointerGrabEnabled: true

    FishUI.MenuItem {
        text: qsTr("Icons")
        iconName: "grid"
        checked: settings.viewMethod === 1
        onTriggered: settings.viewMethod = 1
    }

    FishUI.MenuItem {
        text: qsTr("List")
        iconName: "list"
        checked: settings.viewMethod === 0
        onTriggered: settings.viewMethod = 0
    }

    FishUI.MenuSeparator {}

    FishUI.MenuItem {
        text: qsTr("Name")
        checked: settings.sortMode === 0
        onTriggered: settings.sortMode = 0
    }

    FishUI.MenuItem {
        text: qsTr("Date")
        checked: settings.sortMode === 2
        onTriggered: settings.sortMode = 2
    }

    FishUI.MenuItem {
        text: qsTr("Type")
        checked: settings.sortMode === 3
        onTriggered: settings.sortMode = 3
    }

    FishUI.MenuItem {
        text: qsTr("Size")
        checked: settings.sortMode === 1
        onTriggered: settings.sortMode = 1
    }
}
