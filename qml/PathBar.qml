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
import QtQuick.Window 2.12
import Qt5Compat.GraphicalEffects

import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI

Item {
    id: control

    property string url: ""

    signal itemClicked(string path)
    signal editorAccepted(string path)

    // The path bar is part of the title bar.  Start the Wayland move on the
    // initial press, while retaining the single-click path editor behavior.
    property bool _dragged: false
    property real _pressX: 0
    property real _pressY: 0

    function startWindowMove() {
        if (Window.window && Window.window.helper)
            Window.window.helper.startSystemMove(Window.window)
    }

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.secondBackgroundColor, 1.3)
                                     : FishUI.Theme.secondBackgroundColor
        radius: FishUI.Theme.smallRadius
        z: -1
    }

    // Clicking anywhere that is not a breadcrumb opens the path editor. The
    // view above is not interactive, so its empty area lets the press through.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: {
            _dragged = false
            _pressX = mouse.x
            _pressY = mouse.y
            control.startWindowMove()
        }
        onPositionChanged: {
            if (Math.abs(mouse.x - _pressX) > 4 || Math.abs(mouse.y - _pressY) > 4)
                _dragged = true
        }
        onCanceled: _dragged = true
        onClicked: {
            if (!_dragged)
                openEditor()
        }
    }

    // Keep the original title-bar behavior: once the pointer crosses the
    // drag threshold, take over from a breadcrumb MouseArea and cancel its
    // click.  The immediate move request above is still needed by Wayland;
    // the handler provides the same click-vs-drag semantics as X11.
    DragHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        grabPermissions: PointerHandler.CanTakeOverFromItems
                         | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                         | PointerHandler.ApprovesTakeOverByAnything
        onActiveChanged: if (active) _dragged = true
    }

    ListView {
        id: _pathView
        anchors.fill: parent
        interactive: false
        anchors.topMargin: 2
        anchors.bottomMargin: 2
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

        highlight: Rectangle {
            radius: FishUI.Theme.smallRadius
            color: Qt.rgba(FishUI.Theme.highlightColor.r,
                           FishUI.Theme.highlightColor.g,
                           FishUI.Theme.highlightColor.b, FishUI.Theme.darkMode ? 0.3 : 0.1)
            smooth: true
        }

        delegate: MouseArea {
            id: _item
            height: ListView.view.height - ListView.view.topMargin - ListView.view.bottomMargin
            width: _name.width + FishUI.Units.largeSpacing
            hoverEnabled: true
            z: -1

            property bool selected: index === _pathView.count - 1

            property bool dragged: false
            property real pressX: 0
            property real pressY: 0

            onPressed: {
                dragged = false
                pressX = mouse.x
                pressY = mouse.y
                control.startWindowMove()
            }
            onPositionChanged: {
                if (Math.abs(mouse.x - pressX) > 4 || Math.abs(mouse.y - pressY) > 4)
                    dragged = true
            }
            onCanceled: dragged = true
            onClicked: {
                if (!dragged)
                    control.itemClicked(model.path)
            }

            Rectangle {
                anchors.fill: parent
                radius: FishUI.Theme.smallRadius
                color: _item.pressed ? Qt.rgba(FishUI.Theme.textColor.r,
                                                   FishUI.Theme.textColor.g,
                                                   FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1) :
                       _item.containsMouse ? Qt.rgba(FishUI.Theme.textColor.r,
                                                     FishUI.Theme.textColor.g,
                                                     FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.1 : 0.05) :
                                                              "transparent"

                smooth: true
            }

            Label {
                id: _name
                text: model.name
                anchors.centerIn: parent
                color: selected ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
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
            color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.secondBackgroundColor, 1.7)
                                         : FishUI.Theme.secondBackgroundColor
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
