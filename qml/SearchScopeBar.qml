// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Layouts 1.12
import FishUI 1.0 as FishUI

// Where a search looks: everything in home, or the folder it started from.
Item {
    id: control

    property var folderModel: null
    // The name the folder is shown under, as on its tab.
    property string folderName: ""

    readonly property bool folderIsHome: folderModel
                                         && folderModel.resolvedUrl.toString() === "file://" + folderModel.homePath()

    component ScopeButton: Item {
        id: _scope

        property alias text: _label.text
        property bool selected: false
        signal clicked()

        implicitWidth: _label.implicitWidth + FishUI.Units.largeSpacing * 2
        implicitHeight: 24

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: _scope.selected ? Qt.alpha(FishUI.Theme.highlightColor, FishUI.Theme.darkMode ? 0.3 : 0.15)
                 : _mouse.pressed ? Qt.alpha(FishUI.Theme.textColor, 0.1)
                 : _mouse.containsMouse ? Qt.alpha(FishUI.Theme.textColor, 0.05)
                 : "transparent"
        }

        FishUI.Label {
            id: _label
            anchors.centerIn: parent
            color: _scope.selected ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            width: Math.min(implicitWidth, _scope.width - FishUI.Units.largeSpacing * 2)
        }

        MouseArea {
            id: _mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: _scope.clicked()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.smallSpacing * 1.5
        anchors.rightMargin: FishUI.Units.largeSpacing
        spacing: FishUI.Units.smallSpacing

        FishUI.Label {
            text: qsTr("Search:")
            color: FishUI.Theme.disabledTextColor
            Layout.leftMargin: FishUI.Units.smallSpacing
        }

        ScopeButton {
            text: qsTr("Home")
            selected: control.folderIsHome || (control.folderModel && control.folderModel.searchHome)
            onClicked: control.folderModel.searchHome = true
        }

        ScopeButton {
            Layout.maximumWidth: 240
            visible: !control.folderIsHome
            text: "“" + control.folderName + "”"
            selected: control.folderModel && !control.folderModel.searchHome
            onClicked: control.folderModel.searchHome = false
        }

        Item {
            Layout.fillWidth: true
        }

        FishUI.BusyIndicator {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            visible: control.folderModel && control.folderModel.searchBusy
        }
    }
}
