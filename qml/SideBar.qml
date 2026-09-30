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
import QtQuick 2.12 as Quick
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12

import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

Item {
    id: sideBar

    implicitWidth: 190

    property string title
    property alias currentIndex: listView.currentIndex
    property alias count: listView.count
    property alias model: listView.model
    property string selectedPath
    property int dropIndex: -1
    // Row a drop moves into, as opposed to dropIndex which pins before a row.
    property int dropTargetRow: -1
    property int dropRejectedRow: -1
    property real dropY: 0
    property real pointerY: 0

    signal clicked(string path)
    signal openInNewWindow(string path)
    signal openInNewTab(string path)

    Fm {
        id: _fm
    }

    PlacesModel {
        id: placesModel
        onDeviceOperationFailed: function(message) {
            deviceError.text = message
            deviceError.open()
        }
        onFavoritesChanged: sideBar.updateSelection(sideBar.selectedPath)
        onDeviceSetupDone: sideBar.clicked(filePath)    // Once the device is mounted, act as if it was clicked to open its page
    }

    FishUI.ConfirmDialog {
        id: deviceError
        parentWindow: sideBar.Window.window
        x: sideBar.Window.window ? sideBar.Window.window.x
                                  + Math.round((sideBar.Window.window.width - width) / 2) : 0
        y: sideBar.Window.window ? sideBar.Window.window.y
                                  + Math.round((sideBar.Window.window.height - height) / 2) : 0
        showOnCompleted: false
        title: qsTr("Device operation failed")
        confirmText: qsTr("Close")
        cancelButtonVisible: false
    }

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.backgroundColor, 1.5)
                                     : Qt.darker(FishUI.Theme.backgroundColor, 1.05)
        opacity: 0.7
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        FishUI.Label {
            id: titleLabel
            text: sideBar.title
            color: root.active ? FishUI.Theme.textColor : FishUI.Theme.disabledTextColor
            Layout.fillWidth: true
            Layout.preferredHeight: root.header.height
            Layout.minimumHeight: root.header.height
            Layout.maximumHeight: root.header.height
            elide: Text.ElideRight
            leftPadding: listView.sideInset + FishUI.Units.smallSpacing
            rightPadding: FishUI.Units.largeSpacing + FishUI.Units.smallSpacing
            topPadding: FishUI.Units.smallSpacing
            bottomPadding: 0
            font.pointSize: 12
        }

        ListView {
            id: listView
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.topMargin: FishUI.Units.smallSpacing
            clip: true
            model: placesModel
            header: FishUI.Label {
                width: listView.width
                height: listView.snap(32)
                leftPadding: listView.sideInset + FishUI.Units.smallSpacing
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Favorites")
                color: FishUI.Theme.disabledTextColor
                font.pointSize: 9
                font.bold: true
            }

            // Rounding to whole *logical* pixels is not enough at a fractional scale:
            // at 150% a 37px row pitch is 55.5 device px, so every other row lands on
            // a half pixel and its icon is resampled.
            readonly property real dpr: FishUI.Dpi.ratio
            function snap(v) {
                return Math.round(v * listView.dpr) / listView.dpr
            }

            FishUI.WheelHandler {
                target: listView
            }

            // Not leftMargin/rightMargin: Qt rounds the content item's position to a
            // whole logical pixel, so a snapped fractional margin never takes effect.
            // The inset is applied inside the rows instead.
            readonly property real sideInset: listView.snap(FishUI.Units.smallSpacing * 1.5)

            bottomMargin: listView.snap(FishUI.Units.smallSpacing)
            spacing: listView.snap(3)

            FishUI.ScrollBar.vertical: FishUI.ScrollBar {
                bottomPadding: FishUI.Units.smallSpacing
            }

            section.property: "category"
            section.delegate: FishUI.Label {
                width: ListView.view.width
                height: !section || section === qsTr("Favorites") ? 0 : listView.snap(36)
                // ListView controls delegate visibility, including zero-height sections.
                text: section === qsTr("Favorites") ? "" : section
                leftPadding: listView.snap(listView.sideInset + FishUI.Units.smallSpacing)
                rightPadding: FishUI.Units.smallSpacing
                verticalAlignment: Text.AlignVCenter
                color: FishUI.Theme.disabledTextColor
                font.pointSize: 9
                font.bold: true
            }

            delegate: Item {
                id: _item
                width: ListView.view.width
                height: listView.snap(Math.max(34, FishUI.Units.fontMetrics.height + 12))

                property bool checked: sideBar.currentIndex === index
                property bool dropTarget: sideBar.dropTargetRow === index
                property bool dropRejected: sideBar.dropRejectedRow === index
                opacity: _mouseArea.drag.active ? 0.45 : 1
                MouseArea {
                    id: _mouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    drag.target: index < placesModel.favoriteCount ? dragProxy : null
                    drag.axis: Drag.YAxis
                    onPressed: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            dragProxy.favoriteRow = index
                            dragProxy.x = mouse.x
                            dragProxy.y = _item.mapToItem(sideBar, mouse.x, mouse.y).y
                        }
                    }
                    onReleased: {
                        if (drag.active)
                            dragProxy.Drag.drop()
                        sideBar.clearDrop()
                    }
                    onCanceled: {
                        dragProxy.Drag.cancel()
                        sideBar.clearDrop()
                    }
                    onPositionChanged: function(mouse) {
                        if (drag.active)
                            dragProxy.y = _item.mapToItem(sideBar, mouse.x, mouse.y).y
                    }
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            if (model.isDevice && model.setupNeeded)
                                placesModel.requestSetup(index)
                            else
                                sideBar.clicked(model.path ? model.path : model.url)
                        } else if (mouse.button === Qt.RightButton) {
                            _menu.popup()
                        }
                    }
                }

                Connections {
                    target: _mouseArea.drag
                    function onActiveChanged() {
                        dragProxy.dragging = _mouseArea.drag.active
                    }
                }

                FishUI.DesktopMenu {
                    id: _menu
                    pointerGrabEnabled: true

                    FishUI.MenuItem {
                        text: qsTr("Open In New Tab")
                        enabled: !model.isDevice || !model.setupNeeded
                        onTriggered: sideBar.openInNewTab(model.path ? model.path : model.url)
                    }

                    FishUI.MenuItem {
                        text: qsTr("Open In New Window")

                        onTriggered: {
                            sideBar.openInNewWindow(model.path ? model.path : model.url)
                        }
                    }

                    FishUI.MenuSeparator {
                        visible: index < placesModel.favoriteCount
                    }

                    FishUI.MenuItem {
                        text: qsTr("Remove from Favorites")
                        visible: index < placesModel.favoriteCount
                        onTriggered: placesModel.removeFavorite(index)
                    }

                    FishUI.MenuSeparator {
                        Layout.fillWidth: true
                        visible: _ejectMenuItem.visible || _umountMenuItem.visible
                    }

                    FishUI.MenuItem {
                        id: _ejectMenuItem
                        text: qsTr("Eject")
                        visible: model.isDevice && model.canEject

                        onTriggered: {
                            placesModel.requestEject(index)
                        }
                    }

                    FishUI.MenuItem {
                        id: _umountMenuItem
                        text: qsTr("Unmount")
                        visible: model.isDevice &&
                                 !model.setupNeeded &&
                                 model.path && model.path !== _fm.rootPath()

                        onTriggered: {
                            placesModel.requestTeardown(index)
                        }
                    }
                }

                FishUI.RoundedRectangle {
                    x: listView.sideInset
                    width: _item.width - listView.sideInset * 2
                    height: parent.height
                    radius: FishUI.Theme.mediumRadius
                    continuousCorners: false
                    color: _item.dropRejected ? Qt.alpha("#d94b4b", 0.16) :
                           _mouseArea.pressed || _item.dropTarget || _menu.visible ? Qt.rgba(FishUI.Theme.textColor.r,
                                                       FishUI.Theme.textColor.g,
                                                       FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1) :
                           checked ? Qt.rgba(FishUI.Theme.textColor.r,
                                             FishUI.Theme.textColor.g,
                                             FishUI.Theme.textColor.b, 0.05) : "transparent"
                    border.width: _item.dropTarget || _item.dropRejected || _menu.visible ? 2 : 0
                    border.color: _item.dropRejected ? "#d94b4b" : FishUI.Theme.highlightColor
                }

                // Snap icon coordinates as well as size for fractional display scales.
                FishUI.IconItem {
                    id: _icon
                    x: listView.snap(listView.sideInset + FishUI.Units.smallSpacing)
                    y: listView.snap((_item.height - height) / 2)
                    width: listView.snap(20)
                    height: width

                    themeSize: 20
                    source: model.iconName
                    color: !root.active ? FishUI.Theme.disabledTextColor
                         : checked ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                }

                FishUI.Label {
                    id: _label
                    x: listView.snap(_icon.x + _icon.width + FishUI.Units.smallSpacing)
                    y: listView.snap((_item.height - height) / 2)
                    width: (_ejectButton.visible ? _ejectButton.x : _item.width)
                           - x - listView.snap(FishUI.Units.smallSpacing)
                    text: model.name
                    color: !root.active ? FishUI.Theme.disabledTextColor
                         : checked ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                    elide: Text.ElideRight
                }

                ToolButton {
                    id: _ejectButton
                    x: listView.snap(_item.width - listView.sideInset
                                     - FishUI.Units.smallSpacing - width)
                    y: listView.snap((_item.height - height) / 2)
                    width: listView.snap(28)
                    height: width
                    visible: _ejectMenuItem.visible || _umountMenuItem.visible
                    hoverEnabled: true
                    Accessible.name: _ejectMenuItem.visible ? qsTr("Eject") : qsTr("Unmount")

                    contentItem: Item {
                        FishUI.IconItem {
                            x: listView.snap((parent.width - width) / 2)
                            y: listView.snap((parent.height - height) / 2)
                            width: listView.snap(14)
                            height: width
                            themeSize: 16
                            source: "media-eject-symbolic"
                            color: _icon.color
                            opacity: _ejectButton.hovered ? 0.85 : 0.45
                        }
                    }

                    background: Item {}

                    onClicked: {
                        if (_ejectMenuItem.visible)
                            placesModel.requestEject(index)
                        else
                            placesModel.requestTeardown(index)
                    }
                }
            }
        }
    }

    Item {
        id: dragProxy
        width: 1
        height: 1
        property int favoriteRow: -1
        property bool dragging: false
        Drag.active: dragging
        Drag.source: dragProxy
        Drag.supportedActions: Qt.MoveAction
    }

    Quick.DropArea {
        id: favoritesDrop
        anchors.fill: parent
        z: 10
        property bool internalDrag: false
        property bool canPin: false
        property var urls: []
        property int dropAction: Qt.IgnoreAction
        onEntered: function(drag) {
            internalDrag = drag.source === dragProxy
            urls = !internalDrag && drag.hasUrls ? drag.urls : []
            dropAction = (drag.modifiers & Qt.ControlModifier) ? Qt.CopyAction : Qt.MoveAction
            canPin = urls.length > 0 && placesModel.canAddFavorites(urls)
            drag.accepted = internalDrag || urls.length > 0
            if (drag.accepted) {
                sideBar.positionDrop(drag.y)
                drag.accept(Qt.MoveAction)
            }
        }
        onPositionChanged: function(drag) {
            dropAction = (drag.modifiers & Qt.ControlModifier) ? Qt.CopyAction : Qt.MoveAction
            sideBar.positionDrop(drag.y)
            drag.accepted = sideBar.dropTargetRow >= 0 || sideBar.dropIndex >= 0
            if (drag.accepted)
                drag.accept(Qt.MoveAction)
        }
        onExited: sideBar.clearDrop()
        onDropped: function(drop) {
            var accepted = false
            var action = (drop.modifiers & Qt.ControlModifier) ? Qt.CopyAction : Qt.MoveAction
            if (sideBar.dropTargetRow >= 0)
                accepted = placesModel.dropInto(drop.urls, sideBar.dropTargetRow, action)
            else if (sideBar.dropIndex >= 0 && internalDrag)
                accepted = placesModel.moveFavorite(dragProxy.favoriteRow, sideBar.dropIndex)
            else if (sideBar.dropIndex >= 0)
                accepted = placesModel.addFavorites(drop.urls, sideBar.dropIndex)
            if (accepted)
                drop.accept(sideBar.dropTargetRow >= 0 ? action
                                                       : internalDrag ? Qt.MoveAction : Qt.CopyAction)
            sideBar.clearDrop()
        }
    }

    FishUI.RoundedRectangle {
        z: 11
        visible: sideBar.dropIndex >= 0
        x: listView.sideInset + FishUI.Units.smallSpacing
        y: sideBar.dropY - height / 2
        width: sideBar.width - x - listView.sideInset
        height: 2
        color: FishUI.Theme.highlightColor
        radius: height / 2
        continuousCorners: false
        FishUI.RoundedRectangle {
            x: -3
            anchors.verticalCenter: parent.verticalCenter
            width: 6
            height: 6
            radius: 3
            continuousCorners: false
            color: FishUI.Theme.highlightColor
        }
    }

    Timer {
        interval: 30
        repeat: true
        running: favoritesDrop.containsDrag
        onTriggered: {
            var localY = sideBar.pointerY - listView.y
            var delta = localY < 32 ? -6 : localY > listView.height - 32 ? 6 : 0
            if (delta !== 0) {
                var minimum = listView.originY
                var maximum = Math.max(minimum, listView.contentHeight - listView.height + listView.originY)
                listView.contentY = Math.max(minimum, Math.min(maximum, listView.contentY + delta))
                sideBar.positionDrop(sideBar.pointerY)
            }
        }
    }

    function clearDrop() {
        dropIndex = -1
        dropTargetRow = -1
        dropRejectedRow = -1
    }

    function positionDrop(y) {
        pointerY = y
        var count = placesModel.favoriteCount
        var contentY = y - listView.y + listView.contentY
        dropTargetRow = -1
        dropRejectedRow = -1

        if (!favoritesDrop.internalDrag) {
            var row = listView.indexAt(listView.width / 2, contentY)
            var rowItem = row >= 0 ? listView.itemAtIndex(row) : null
            if (rowItem) {
                // Favorite rows keep their edges for pinning between rows.
                var edge = favoritesDrop.canPin && row < count ? rowItem.height / 4 : 0
                if (contentY >= rowItem.y + edge && contentY <= rowItem.y + rowItem.height - edge) {
                    dropIndex = -1
                    if (placesModel.canDropInto(favoritesDrop.urls, row,
                                                favoritesDrop.dropAction))
                        dropTargetRow = row
                    else
                        dropRejectedRow = row
                    return
                }
            }
            if (!favoritesDrop.canPin || row >= count) {
                dropIndex = -1
                return
            }
        }

        var boundary = listView.headerItem.y + listView.headerItem.height
        dropIndex = count
        for (var i = 0; i < count; ++i) {
            var item = listView.itemAtIndex(i)
            if (!item)
                continue
            if (contentY < item.y + item.height / 2) {
                dropIndex = i
                boundary = item.y
                break
            }
            boundary = item.y + item.height
        }
        dropY = listView.y + boundary - listView.contentY
        if (contentY > boundary + 24 || dropY < listView.y || dropY > listView.y + listView.height)
            dropIndex = -1
    }

    function updateSelection(path) {
        selectedPath = path
        listView.currentIndex = -1

        var selectedUrl = path.toString()
        if (selectedUrl.indexOf("trash:/") === 0)
            selectedUrl = "trash:/"

        for (var i = 0; i < listView.count; ++i) {
            var place = listView.model.get(i)
            var placeUrl = place.url.toString()
            if (placeUrl.indexOf("trash:/") === 0)
                placeUrl = "trash:/"

            if (selectedUrl === place.path || selectedUrl === placeUrl) {
                listView.currentIndex = i
                break
            }
        }
    }
}
