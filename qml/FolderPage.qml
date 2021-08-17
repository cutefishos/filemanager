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
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.0
import Qt.labs.platform 1.0

import Cutefish.FileManager 1.0 as FM
import FishUI 1.0 as FishUI

import "./Dialogs"

Item {
    id: folderPage

    property alias currentUrl: dirModel.url
    property Item currentView: _viewLoader.item
    property int statusBarHeight: 22

    signal requestPathEditor()

    onCurrentUrlChanged: {
        _viewLoader.item.reset()
        _viewLoader.item.forceActiveFocus()
    }

    // Global Menu
    MenuBar {
        id: appMenu

        Menu {
            title: qsTr("File")

            MenuItem {
                text: qsTr("New Folder")
                onTriggered: dirModel.newFolder()
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Properties")
                onTriggered: dirModel.openPropertiesDialog()
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Quit")
                onTriggered: Qt.quit()
            }
        }

        Menu {
            title: qsTr("Edit")

            MenuItem {
                text: qsTr("Select All")
                onTriggered: dirModel.selectAll()
            }

            MenuSeparator {}

            MenuItem {
                text: qsTr("Cut")
                onTriggered: dirModel.cut()
            }

            MenuItem {
                text: qsTr("Copy")
                onTriggered: dirModel.copy()
            }

            MenuItem {
                text: qsTr("Paste")
                onTriggered: dirModel.paste()
            }
        }

        Menu {
            title: qsTr("Help")

            MenuItem {
                text: qsTr("About")
            }
        }
    }

    Rectangle {
        id: _background
        anchors.fill: parent
        radius: FishUI.Theme.smallRadius
        color: FishUI.Theme.secondBackgroundColor

        Rectangle {
            id: _topRightRect
            anchors.right: parent.right
            anchors.top: parent.top
            height: FishUI.Theme.smallRadius
            width: FishUI.Theme.smallRadius
            color: FishUI.Theme.secondBackgroundColor
        }

        Rectangle {
            id: _bottomLeftRect
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            height: FishUI.Theme.smallRadius
            width: FishUI.Theme.smallRadius
            color: FishUI.Theme.secondBackgroundColor
        }
    }

    Label {
        id: _fileTips
        text: qsTr("Empty folder")
        font.pointSize: 15
        anchors.centerIn: parent
        visible: false
    }

    FM.FolderModel {
        id: dirModel
        viewAdapter: viewAdapter

        Component.onCompleted: {
            if (arg)
                dirModel.url = arg
            else
                dirModel.url = dirModel.homePath()
        }
    }

    FM.ItemViewAdapter {
        id: viewAdapter
        adapterView: _viewLoader.item
        adapterModel: _viewLoader.item.positioner ? _viewLoader.item.positioner : dirModel
        adapterIconSize: 40
        adapterVisibleArea: Qt.rect(_viewLoader.item.contentX, _viewLoader.item.contentY,
                                    _viewLoader.item.contentWidth, _viewLoader.item.contentHeight)
    }

    FishUI.DesktopMenu {
        id: folderMenu

        MenuItem {
            text: qsTr("Open")
            onTriggered: dirModel.openSelected()
        }

        MenuItem {
            text: qsTr("Properties")
            onTriggered: dirModel.openPropertiesDialog()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.bottomMargin: 2
        spacing: 0

        Loader {
            id: _viewLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: switch (settings.viewMethod) {
                             case 0: return _listViewComponent
                             case 1: return _gridViewComponent
                             }

            onSourceComponentChanged: {
                // Focus
                _viewLoader.item.forceActiveFocus()

                // ShortCut
                shortCut.install(_viewLoader.item)
            }
        }

        Item {
            visible: true
            height: statusBarHeight
        }
    }

    Item {
        id: _statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: statusBarHeight
        z: 999

//        Rectangle {
//            anchors.fill: parent
//            color: FishUI.Theme.backgroundColor
//            opacity: 0.7
//        }

        MouseArea {
            anchors.fill: parent
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: FishUI.Units.smallSpacing
            anchors.rightMargin: FishUI.Units.smallSpacing
            // anchors.bottomMargin: 1
            spacing: FishUI.Units.largeSpacing

            Label {
                Layout.alignment: Qt.AlignLeft
                font.pointSize: 10
                text: dirModel.count === 1 ? qsTr("%1 item").arg(dirModel.count)
                                           : qsTr("%1 items").arg(dirModel.count)
            }

            Label {
                Layout.alignment: Qt.AlignLeft
                font.pointSize: 10
                text: qsTr("%1 selected").arg(dirModel.selectionCount)
                visible: dirModel.selectionCount >= 1
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignRight
                text: qsTr("Empty Trash")
                font.pointSize: 10
                onClicked: dirModel.emptyTrash()
                visible: dirModel.url === "trash:///"
                focusPolicy: Qt.NoFocus
            }
        }
    }

    function rename() {
        _viewLoader.item.rename()
    }

    Component.onCompleted: {
        dirModel.requestRename.connect(rename)
    }

    Component {
        id: _gridViewComponent

        FolderGridView {
            id: _gridView
            model: dirModel
            delegate: FolderGridItem {}

            leftMargin: FishUI.Units.smallSpacing
            rightMargin: FishUI.Units.largeSpacing
            topMargin: 0
            bottomMargin: FishUI.Units.smallSpacing

            onIconSizeChanged: {
                // Save
                settings.gridIconSize = _gridView.iconSize
            }

            onCountChanged: {
                _fileTips.visible = count === 0
            }
        }
    }

    Component {
        id: _listViewComponent

        FolderListView {
            id: _folderListView
            model: dirModel

            topMargin: FishUI.Units.smallSpacing
            leftMargin: FishUI.Units.largeSpacing
            rightMargin: FishUI.Units.largeSpacing
            bottomMargin: FishUI.Units.smallSpacing
            spacing: FishUI.Units.largeSpacing

            onCountChanged: {
                _fileTips.visible = count === 0
            }

            delegate: FolderListItem {}
        }
    }

    Component {
        id: rubberBandObject

        FM.RubberBand {
            id: rubberBand

            width: 0
            height: 0
            z: 99999
            color: FishUI.Theme.highlightColor

            function close() {
                opacityAnimation.restart()
            }

            OpacityAnimator {
                id: opacityAnimation
                target: rubberBand
                to: 0
                from: 1
                duration: 150

                easing {
                    bezierCurve: [0.4, 0.0, 1, 1]
                    type: Easing.Bezier
                }

                onFinished: {
                    rubberBand.visible = false
                    rubberBand.enabled = false
                    rubberBand.destroy()
                }
            }
        }
    }

    FM.ShortCut {
        id: shortCut

        onOpen: {
            dirModel.openSelected()
        }
        onCopy: {
            dirModel.copy()
        }
        onCut: {
            dirModel.cut()
        }
        onPaste: {
            dirModel.paste()
        }
        onRename: {
            dirModel.requestRename()
        }
        onOpenPathEditor: {
            folderPage.requestPathEditor()
        }
        onSelectAll: {
            dirModel.selectAll()
        }
        onBackspace: {
            dirModel.up()
        }
        onDeleteFile: {
            dirModel.keyDeletePress()
        }
    }

    function openUrl(url) {
        dirModel.url = url
        _viewLoader.item.forceActiveFocus()
    }

    function goBack() {
        dirModel.goBack()
    }

    function goForward() {
        dirModel.goForward()
    }
}
