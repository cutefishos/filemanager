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
import QtQuick.Window 2.12

import QtCore

import Cutefish.FileManager 1.0 as FM
import FishUI 1.0 as FishUI
import "../"

Item {
    id: rootItem

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    readonly property string screenName: (typeof desktopView !== "undefined" && desktopView)
                                         ? String(desktopView.screenName).replace(/[\/\\]/g, "_") : ""
    readonly property string legacyLayoutKey: "%1_%2x%3".arg(screenName)
                                              .arg(_folderView.gridColumns).arg(_folderView.gridRows)
    property bool restoringLayout: false
    property bool layoutLoaded: false

    GlobalSettings {
        id: globalSettings
    }

    Settings {
        id: layoutSettings
        location: globalSettings.location
        category: "DesktopIconLayout"
    }

    Timer {
        id: saveLayoutTimer
        interval: 500
        onTriggered: rootItem.saveLayout()
    }

    Timer {
        id: restoreLayoutTimer
        interval: 150
        onTriggered: rootItem.loadLayout()
    }

    onScreenNameChanged: {
        layoutLoaded = false;
        saveLayoutTimer.stop()
        restoreLayoutTimer.restart()
    }

    Component.onCompleted: {
        loadLayout()
        takeFocus()
    }

    // The desktop is built by DesktopView and only then parented into the
    // window, so `focus: true` on the view never reaches the focus chain:
    // claim it explicitly, and again whenever the window is activated.
    function takeFocus() {
        // Not while a name is being edited: the editor is a child of the view,
        // and taking the focus back would close it.
        if (_folderView.editor && _folderView.editor.targetItem)
            return

        _folderView.forceActiveFocus()
    }

    Connections {
        target: rootItem.Window.window

        function onActiveChanged() {
            if (rootItem.Window.window.active)
                rootItem.takeFocus()
        }
    }

    function loadLayout() {
        if (_folderView.width <= 0 || _folderView.height <= 0 || layoutLoaded)
            return

        restoringLayout = true

        var raw = layoutSettings.value("layout.v3." + screenName, "")
        if (!raw)
            raw = layoutSettings.value("layout.v2." + legacyLayoutKey, "")
        if (!raw)
            raw = layoutSettings.value("latest.v2." + screenName, "")
        var legacy = false
        if (!raw) {
            raw = layoutSettings.value("layout." + legacyLayoutKey, "")
            legacy = !!raw
        }

        if (raw) {
            try {
                var stored = JSON.parse(raw)
                if (stored instanceof Array && stored.length >= 5) {
                    // Legacy headers counted occupied columns, not the screen grid.
                    if (legacy)
                        stored[0] = String(_folderView.gridColumns)
                    _folderView.positioner.positions = stored
                }
            } catch (e) {
                console.warn("Desktop: ignoring unreadable icon layout for", screenName, e)
            }
        }

        layoutLoaded = true
        restoringLayout = false
        saveLayoutTimer.restart()
    }

    function saveLayout() {
        if (!layoutLoaded || restoringLayout || restoreLayoutTimer.running)
            return

        var positions = _folderView.positioner.positions

        // Fewer than one full record means the folder has not been listed yet;
        // saving that would wipe the layout of an empty desktop.
        if (positions.length < 5)
            return

        layoutSettings.setValue("layout.v3." + screenName, JSON.stringify(positions))
    }

    Connections {
        target: _folderView.positioner

        function onPositionsChanged() {
            if (!rootItem.restoringLayout)
                saveLayoutTimer.restart()
        }
    }

    FM.FolderModel {
        id: dirModel
        url: desktopPath()
        isDesktop: true
        sortMode: -1
        viewAdapter: viewAdapter

        Component.onCompleted: desktopView.folderModel = dirModel

        onCurrentIndexChanged: {
            _folderView.currentIndex = _folderView.positioner.mapFromSource(dirModel.currentIndex)
        }
    }

    FM.ItemViewAdapter {
        id: viewAdapter
        adapterView: _folderView
        adapterModel: _folderView.positioner
        adapterIconSize: _folderView.iconSize
        adapterVisibleArea: Qt.rect(_folderView.contentX, _folderView.contentY,
                                    _folderView.contentWidth, _folderView.contentHeight)
    }

    FolderContextMenu {
        id: desktopMenu
        folderModel: dirModel
    }

    ArchiveProgressDialog {
        id: archiveProgressDialog
        archiveModel: dirModel
        hostWindow: rootItem.Window.window
    }

    FolderGridView {
        id: _folderView
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: desktopView.topMargin + 12
        anchors.rightMargin: desktopView.rightMargin + 12
        readonly property real availableWidth: Math.max(0, rootItem.width - desktopView.leftMargin - desktopView.rightMargin - 24)
        readonly property real availableHeight: Math.max(0, rootItem.height - desktopView.topMargin - desktopView.bottomMargin - 24)
        width: Math.floor(availableWidth / cellWidth) * cellWidth
        height: Math.floor(availableHeight / cellHeight) * cellHeight
        visible: width > 0 && height > 0
        onWidthChanged: {
            if (!rootItem.layoutLoaded)
                restoreLayoutTimer.restart()
        }
        onHeightChanged: {
            if (!rootItem.layoutLoaded)
                restoreLayoutTimer.restart()
        }
        LayoutMirroring.enabled: false
        layoutDirection: Qt.LeftToRight

        isDesktopView: true
        interactionSurface: rootItem
        iconSize: globalSettings.desktopIconSize
        maximumIconSize: globalSettings.maximumIconSize
        minimumIconSize: 22
        focus: true
        model: _folderView.positioner

        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

        flow: GridView.FlowTopToBottom

        delegate: FolderGridItem {}
        useCustomContextMenu: true

        onContextMenuRequested: desktopMenu.popup()

        onIconSizeChanged: {
            globalSettings.desktopIconSize = _folderView.iconSize
        }

        onActiveFocusChanged: {
            // The context menu is a separate popup window. Opening it moves
            // focus away from the desktop, but the selected item must remain
            // selected while the menu builds and handles its actions.
            if (!activeFocus && !desktopMenu.visible) {
                _folderView.cancelRename()
                dirModel.clearSelection()
            }
        }

        Component.onCompleted: {
            dirModel.requestRename.connect(rename)
        }
    }

    FM.ShortCut {
        id: shortCut

        Component.onCompleted: {
            shortCut.install(_folderView)
        }

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
        onDeleteFile: {
            dirModel.keyDeletePress()
        }
        onKeyPressed: {
            dirModel.keyboardSearch(text)
        }
        onShowHidden: {
            dirModel.showHiddenFiles = !dirModel.showHiddenFiles
        }
        onUndo: {
            dirModel.undo()
        }
        onRedo: {
            dirModel.redo()
        }
    }

    Component {
        id: rubberBandObject

        FM.RubberBand {
            id: rubberBand

            width: 0
            height: 0
            z: 99999
            color: "white"

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
}
