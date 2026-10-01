// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Controls 2.12

import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI

import "./Controls"

Item {
    id: control

    property string url: ""

    signal itemClicked(string path)
    signal editorAccepted(string path)
    signal openInNewTab(string path)
    signal openInNewWindow(string path)

    property bool _dragged: false
    property real _pressX: 0
    property real _pressY: 0

    // Clicking anywhere that is not a breadcrumb opens the path editor. The
    // view above is not interactive, so its empty area lets the press through.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            _dragged = false
            _pressX = mouse.x
            _pressY = mouse.y
        }
        onPositionChanged: function(mouse) {
            if (Math.abs(mouse.x - _pressX) > 4 || Math.abs(mouse.y - _pressY) > 4)
                _dragged = true
        }
        onCanceled: _dragged = true
        onClicked: {
            if (!_dragged)
                openEditor()
        }
    }

    Breadcrumbs {
        id: _pathView
        anchors.fill: parent
        model: _pathBarModel
        openActionsEnabled: true

        onItemClicked: function(path) { control.itemClicked(path) }
        onOpenInNewTab: function(path) { control.openInNewTab(path) }
        onOpenInNewWindow: function(path) { control.openInNewWindow(path) }
    }

    FishUI.TextField {
        id: _pathEditor
        anchors.fill: parent
        visible: false
        selectByMouse: true
        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase

        text: _pathBarModel.url
        color: FishUI.Theme.darkMode ? "white" : "black"

        background: FishUI.RoundedRectangle {
            radius: FishUI.Theme.smallRadius
            continuousCorners: false
            color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.secondBackgroundColor, 1.7)
                                         : FishUI.Theme.secondBackgroundColor
            border.width: 1
            border.color: FishUI.Theme.highlightColor
        }

        onAccepted: {
            control.editorAccepted(text)
            closeEditor()
        }

        Keys.onPressed: function(event) {
            if (_completion.handleKey(event))
                event.accepted = true
            else if (event.key === Qt.Key_Escape)
                focus = false
        }

        onActiveFocusChanged: {
            if (!activeFocus) {
                closeEditor()
            }
        }
    }

    PathCompletion {
        id: _completion
        editor: _pathEditor

        onActivated: function(path) {
            control.editorAccepted(path)
            closeEditor()
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
        _completion.reset()
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
