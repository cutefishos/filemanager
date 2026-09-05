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
import Qt5Compat.GraphicalEffects

import Cutefish.FileManager 1.0 as FM
import FishUI 1.0 as FishUI

import "./Dialogs"

Item {
    id: folderPage

    property alias currentUrl: dirModel.url
    property alias model: dirModel
    property alias canGoBack: dirModel.canGoBack
    property alias canGoForward: dirModel.canGoForward
    property Item currentView: _viewLoader.item
    property int headerHeight: 0
    property int bottomNavigationHeight: 0
    property string initialUrl: ""
    readonly property string tabTitle: {
        var path = currentUrl.toString()
        if (path.indexOf("trash:/") === 0)
            return qsTr("Trash")
        if (path.indexOf("file://") === 0)
            path = path.substring(7)
        path = path.replace(/\/+$/, "")
        var name = path.substring(path.lastIndexOf("/") + 1)
        try { return decodeURIComponent(name) || "/" } catch (error) { return name || "/" }
    }

    signal requestPathEditor()
    signal openInNewTab(string path)
    signal closeRequested()

    onCurrentUrlChanged: {
        if (!_viewLoader.item)
            return

        _viewLoader.item.reset()
        focusView()
    }


    Rectangle {
        id: _background
        anchors.fill: parent
        color: FishUI.Theme.secondBackgroundColor
    }

    Label {
        id: _fileTips
        text: qsTr("Empty folder")
        font.pointSize: 15
        anchors.centerIn: parent
        visible: dirModel.status === FM.FolderModel.Ready
                 && _viewLoader.status === Loader.Ready
                 && dirModel.count === 0
    }

    FM.FolderModel {
        id: dirModel
        viewAdapter: viewAdapter
        sortMode: settings.sortMode
        // showHiddenFiles: settings.showHiddenFiles

        Component.onCompleted: {
            if (folderPage.initialUrl)
                dirModel.url = folderPage.initialUrl
            else
                dirModel.url = dirModel.homePath()
        }

        onOpenTabRequested: function(path) { folderPage.openInNewTab(path) }

        // For new folder rename.
        onCurrentIndexChanged: {
            if (_viewLoader.item)
                _viewLoader.item.currentIndex = dirModel.currentIndex
        }
    }

    Connections {
        target: dirModel

        function onNotification(text) {
            root.showPassiveNotification(text, 3000)
        }

        // Scroll to item.
        function onScrollToItem(index) {
            if (_viewLoader.item)
                _viewLoader.item.currentIndex = index
        }
    }

    FM.ItemViewAdapter {
        id: viewAdapter
        adapterView: _viewLoader.item
        adapterModel: _viewLoader.item && _viewLoader.item.positioner ? _viewLoader.item.positioner : dirModel
        adapterIconSize: 40
        adapterVisibleArea: _viewLoader.item ? Qt.rect(_viewLoader.item.contentX, _viewLoader.item.contentY,
                                    _viewLoader.item.contentWidth, _viewLoader.item.contentHeight) : Qt.rect(0, 0, 0, 0)
    }

    FolderContextMenu {
        id: folderMenu
        folderModel: dirModel
    }

    ColumnLayout {
        anchors.fill: parent
        // Same breathing room the sidebar leaves between its title and its list.
        anchors.topMargin: folderPage.headerHeight
        anchors.bottomMargin: 2 + folderPage.bottomNavigationHeight
        spacing: 0

        Loader {
            id: _viewLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            asynchronous: true
            sourceComponent: switch (settings.viewMethod) {
                             case 0: return _listViewComponent
                             case 1: return _gridViewComponent
                             }

            onLoaded: {
                folderPage.focusView()
                shortCut.install(_viewLoader.item)
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
            useCustomContextMenu: true

            onContextMenuRequested: folderMenu.popup()

            leftMargin: FishUI.Units.smallSpacing
            rightMargin: FishUI.Units.largeSpacing
            topMargin: 0
            bottomMargin: FishUI.Units.smallSpacing

            onIconSizeChanged: {
                // Save
                settings.gridIconSize = _gridView.iconSize
            }

        }
    }

    Component {
        id: _listViewComponent

        FolderListView {
            id: _folderListView
            model: dirModel
            useCustomContextMenu: true

            onContextMenuRequested: folderMenu.popup()

            topMargin: FishUI.Units.smallSpacing
            leftMargin: FishUI.Units.largeSpacing
            rightMargin: FishUI.Units.largeSpacing
            bottomMargin: FishUI.Units.smallSpacing
            spacing: FishUI.Units.largeSpacing

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
        onRefresh: {
            dirModel.refresh()
        }
        onKeyPressed: {
            dirModel.keyboardSearch(text)
        }
        onShowHidden: {
            dirModel.showHiddenFiles = !dirModel.showHiddenFiles
        }
        onClose: {
            folderPage.closeRequested()
        }
        onUndo: {
            dirModel.undo()
        }
    }

    function openUrl(url) {
        dirModel.url = url
        focusView()
    }

    function focusView() {
        if (visible && _viewLoader.item)
            _viewLoader.item.forceActiveFocus()
    }

    function goBack() {
        dirModel.goBack()
    }

    function goForward() {
        dirModel.goForward()
    }
}
