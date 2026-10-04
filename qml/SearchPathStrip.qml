// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

import "./Controls"

// Under search results: where the selected result is. A crumb opens its folder;
// the result's own crumb opens its folder with it selected.
Item {
    id: control

    property var folderModel: null
    // The view running under the strip, blurred behind it as under the header.
    property Flickable view: null

    signal openFolder(string path)

    Loader {
        anchors.fill: parent
        active: control.view !== null

        sourceComponent: FishUI.HeaderBackdrop {
            view: control.view
            edge: Qt.BottomEdge
            viewPosition: Qt.point(control.x, control.y)
            baseColor: FishUI.Theme.secondBackgroundColor
        }
    }

    // Rows scroll beneath the strip; presses on it must not reach them. Wheel
    // events still pass through.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }

    Breadcrumbs {
        anchors.fill: parent
        // Crumb text then starts where the scope bar's label does.
        anchors.leftMargin: FishUI.Units.smallSpacing / 2
        anchors.rightMargin: FishUI.Units.largeSpacing
        visible: _model.url.length > 0
        model: _model

        onItemClicked: function(path) {
            if (path === _model.url)
                control.folderModel.showInFolder()
            else
                control.openFolder(path)
        }
    }

    PathBarModel {
        id: _model
        url: control.folderModel ? control.folderModel.selectedPath : ""
    }
}
