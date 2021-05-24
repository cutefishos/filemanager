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
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI

Item {
    id: control

    property string url: ""

    signal itemClicked(string path)
    signal editorAccepted(string path)

    ListView {
        id: _pathView
        anchors.fill: parent
        model: _pathBarModel
        orientation: Qt.Horizontal
        layoutDirection: Qt.LeftToRight
        clip: true

        leftMargin: 3
        rightMargin: 3
        spacing: FishUI.Units.smallSpacing

        onCountChanged: {
            _pathView.currentIndex = _pathView.count - 1
            _pathView.positionViewAtEnd()
        }

        Rectangle {
            anchors.fill: parent
            color: FishUI.Theme.backgroundColor
            radius: FishUI.Theme.smallRadius
            z: -1
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: openEditor()
            z: -1
        }

        delegate: MouseArea {
            id: _item
            height: ListView.view.height
            width: _name.width + FishUI.Units.largeSpacing
            z: -1

            property bool selected: index === _pathView.count - 1

            onClicked: control.itemClicked(model.path)

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 2
                anchors.bottomMargin: 2
                color: FishUI.Theme.highlightColor
                radius: FishUI.Theme.smallRadius
                visible: selected
            }

            Label {
                id: _name
                text: model.name
                color: selected ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
                anchors.centerIn: parent
            }
        }
    }

    TextField {
        id: _pathEditor
        anchors.fill: parent
        visible: false
        selectByMouse: true
        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase

        text: _pathBarModel.url
        color: FishUI.Theme.darkMode ? "white" : "black"

        background: Rectangle {
            radius: FishUI.Theme.smallRadius
            color: FishUI.Theme.darkMode ? Qt.darker(FishUI.Theme.backgroundColor, 1.1) : "white"
            border.width: 1
            border.color: FishUI.Theme.highlightColor
        }

        onAccepted: {
            control.editorAccepted(text)
            closeEditor()
        }

        Keys.onPressed: {
            if (event.key === Qt.Key_Escape)
                focus = false
        }

        onActiveFocusChanged: {
            if (!activeFocus) {
                closeEditor()
            }
        }
    }

    PathBarModel {
        id: _pathBarModel
    }

    function updateUrl(url) {
        control.url = url
        _pathBarModel.url = url
    }

    function openEditor() {
        _pathEditor.text = _pathBarModel.url
        _pathEditor.visible = true
        _pathEditor.forceActiveFocus()
        _pathEditor.selectAll()
        _pathView.visible = false
    }

    function closeEditor() {
        _pathEditor.visible = false
        _pathView.visible = true
    }
}
