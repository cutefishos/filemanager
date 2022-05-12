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
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0 as FM
import FishUI 1.0 as FishUI
import "../"

Item {
    id: rootItem

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    GlobalSettings {
        id: globalSettings
    }

    Wallpaper {
        anchors.fill: parent
    }

    FM.FolderModel {
        id: dirModel
        url: desktopPath()
        isDesktop: true
        sortMode: -1
        viewAdapter: viewAdapter

        onCurrentIndexChanged: {
            _folderView.currentIndex = dirModel.currentIndex
        }
    }

    FM.ItemViewAdapter {
        id: viewAdapter
        adapterView: _folderView
        adapterModel: dirModel
        adapterIconSize: 40
        adapterVisibleArea: Qt.rect(_folderView.contentX, _folderView.contentY,
                                    _folderView.contentWidth, _folderView.contentHeight)
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
        model: dirModel

        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

        // Handle for topbar
        topMargin: 28

        // From dock
        leftMargin: Dock.leftMargin
        rightMargin: Dock.rightMargin
        bottomMargin: Dock.bottomMargin

        flow: GridView.FlowTopToBottom

        delegate: FolderGridItem {}

        onIconSizeChanged: {
            globalSettings.desktopIconSize = _folderView.iconSize
        }

        onActiveFocusChanged: {
            if (!activeFocus) {
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
