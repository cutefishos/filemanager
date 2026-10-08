// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import FishUI 1.0 as FishUI

FishUI.DesktopMenu {
    id: control

    pointerGrabEnabled: true

    // The trash sorts apart and by its own columns.
    property bool trash: false
    readonly property int sortMode: trash ? settings.trashSortMode : settings.sortMode

    function sortBy(mode) {
        if (trash)
            settings.trashSortMode = mode
        else
            settings.sortMode = mode
    }

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
        checked: control.sortMode === 0
        onTriggered: control.sortBy(0)
    }

    FishUI.MenuItem {
        text: qsTr("Original Location")
        visible: control.trash
        checked: control.sortMode === 7
        onTriggered: control.sortBy(7)
    }

    FishUI.MenuItem {
        text: control.trash ? qsTr("Date Deleted") : qsTr("Date")
        checked: control.sortMode === (control.trash ? 8 : 2)
        onTriggered: control.sortBy(control.trash ? 8 : 2)
    }

    FishUI.MenuItem {
        text: qsTr("Type")
        visible: !control.trash
        checked: control.sortMode === 3
        onTriggered: control.sortBy(3)
    }

    FishUI.MenuItem {
        text: qsTr("Size")
        checked: control.sortMode === 1
        onTriggered: control.sortBy(1)
    }
}
