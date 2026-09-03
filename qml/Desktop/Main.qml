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

    // Icon positions are stored per screen and per grid size, so switching
    // resolution -- or icon size -- picks up the arrangement that belongs to it
    // and switching back restores the previous one untouched.
    readonly property string screenName: (typeof desktopView !== "undefined" && desktopView)
                                         ? String(desktopView.screenName).replace(/[\/\\]/g, "_") : ""
    readonly property string layoutKey: (_folderView.width > 0 && _folderView.height > 0)
                                        ? "%1_%2x%3".arg(screenName).arg(_folderView.gridColumns).arg(_folderView.gridRows)
                                        : ""

    property bool restoringLayout: false

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

    onLayoutKeyChanged: loadLayout()

    Component.onCompleted: loadLayout()

    function loadLayout() {
        if (!layoutKey)
            return

        restoringLayout = true

        var raw = layoutSettings.value("layout." + layoutKey, "")

        if (raw) {
            try {
                var stored = JSON.parse(raw)
                if (stored instanceof Array && stored.length >= 5)
                    _folderView.positioner.positions = stored
            } catch (e) {
                console.warn("Desktop: ignoring unreadable icon layout for", layoutKey, e)
            }
        }

        restoringLayout = false

        // Whatever we ended up with -- the stored layout, or the previous one
        // reflowed into the new grid -- belongs to this profile from now on.
        saveLayoutTimer.restart()
    }

    function saveLayout() {
        if (!layoutKey || restoringLayout)
            return

        var positions = _folderView.positioner.positions

        // Fewer than one full record means the folder has not been listed yet;
        // saving that would wipe the layout of an empty desktop.
        if (positions.length < 5)
            return

        layoutSettings.setValue("layout." + layoutKey, JSON.stringify(positions))
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

        onCurrentIndexChanged: {
            _folderView.currentIndex = _folderView.positioner.mapFromSource(dirModel.currentIndex)
        }
    }

    FM.ItemViewAdapter {
        id: viewAdapter
        adapterView: _folderView
        adapterModel: _folderView.positioner
        adapterIconSize: 40
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

    MouseArea {
        anchors.fill: parent
        onClicked: _folderView.forceActiveFocus()
    }

    FolderGridView {
        id: _folderView
        anchors.fill: parent

        isDesktopView: true
        iconSize: globalSettings.desktopIconSize
        maximumIconSize: globalSettings.maximumIconSize
        minimumIconSize: 22
        focus: true
        model: _folderView.positioner

        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

        // Handle for topbar
        topMargin: 28

        // From dock
        leftMargin: Dock.leftMargin
        rightMargin: Dock.rightMargin
        bottomMargin: Dock.bottomMargin

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
}
