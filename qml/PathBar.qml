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
    // The current tab's folder model, which the editor searches.
    property var folderModel: null

    readonly property bool searching: folderModel !== null && folderModel.searchText.length > 0
    // The editor has the keys; a search keeps it shown without them.
    property bool editing: false

    signal itemClicked(string path)
    signal editorAccepted(string path)
    signal openInNewTab(string path)
    signal openInNewWindow(string path)
    // The editor gave the keys back to the folder view.
    signal finished()

    property bool _dragged: false
    property real _pressX: 0
    property real _pressY: 0

    // File names cannot hold a slash, so text with one is a path to go to.
    function isPath(text) {
        return text.indexOf("/") >= 0 || text.charAt(0) === "~"
                || /^(trash|file):/.test(text)
    }

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
        visible: !_pathEditor.visible

        onItemClicked: function(path) { control.itemClicked(path) }
        onOpenInNewTab: function(path) { control.openInNewTab(path) }
        onOpenInNewWindow: function(path) { control.openInNewWindow(path) }
    }

    FishUI.TextField {
        id: _pathEditor
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        // Lower than the header buttons, centred on them.
        height: 28
        visible: control.editing || control.searching
        selectByMouse: true
        inputMethodHints: Qt.ImhNoAutoUppercase
        placeholderText: qsTr("Search or Enter Path")
        rightPadding: _clear.visible ? width - _clear.x + FishUI.Units.smallSpacing
                                     : FishUI.Units.largeSpacing

        color: FishUI.Theme.darkMode ? "white" : "black"

        background: FishUI.RoundedRectangle {
            radius: FishUI.Theme.smallRadius
            continuousCorners: false
            color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.secondBackgroundColor, 1.7)
                                         : FishUI.Theme.secondBackgroundColor
            border.width: 1
            // A search left showing keeps a faint outline, so it still reads as a field.
            border.color: _pathEditor.activeFocus ? FishUI.Theme.highlightColor
                                                  : Qt.alpha(FishUI.Theme.textColor, 0.15)
        }

        onTextEdited: {
            if (control.isPath(text)) {
                _debounce.stop()
                if (control.searching)
                    control.folderModel.searchText = ""
            } else {
                _debounce.restart()
            }
        }

        onAccepted: {
            _debounce.stop()

            if (control.isPath(text)) {
                control.editorAccepted(text)
                closeEditor()
            } else if (control.folderModel) {
                control.folderModel.searchText = text
            }

            control.finished()
        }

        Keys.onPressed: function(event) {
            if (_completion.handleKey(event)) {
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                endSearch()
                control.finished()
                event.accepted = true
            }
        }

        onActiveFocusChanged: {
            if (!activeFocus)
                control.editing = false
        }

        FishUI.Icon {
            id: _clear
            anchors.right: parent.right
            anchors.rightMargin: FishUI.Units.smallSpacing * 1.5
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            name: "clear"
            color: FishUI.Theme.disabledTextColor
            visible: control.searching

            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                onClicked: {
                    endSearch()
                    control.finished()
                }
            }
        }
    }

    // Waits for typing to stop before searching.
    Timer {
        id: _debounce
        interval: 150
        onTriggered: {
            if (control.folderModel)
                control.folderModel.searchText = _pathEditor.text
        }
    }

    Connections {
        target: control.folderModel

        // Ended from elsewhere: Back, or a folder opened from the results.
        function onSearchTextChanged() {
            if (!_debounce.running && !_pathEditor.activeFocus)
                _pathEditor.text = control.folderModel.searchText
        }
    }

    PathCompletion {
        id: _completion
        editor: _pathEditor
        active: control.isPath(_pathEditor.text)

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
        _debounce.stop()
        _completion.reset()
        _pathEditor.text = _pathBarModel.url
        control.editing = true
        _pathEditor.forceActiveFocus()
        _pathEditor.selectAll()
    }

    function openSearch() {
        _debounce.stop()
        _completion.reset()
        _pathEditor.text = control.searching ? control.folderModel.searchText : ""
        control.editing = true
        _pathEditor.forceActiveFocus()
        _pathEditor.selectAll()
    }

    function endSearch() {
        _debounce.stop()
        control.editing = false
        _pathEditor.text = ""
        if (control.searching)
            control.folderModel.searchText = ""
    }

    // Shows the current tab's search, if it has one, without the keys.
    function closeEditor() {
        _debounce.stop()
        control.editing = false
        _pathEditor.text = control.searching ? control.folderModel.searchText : ""
    }
}
