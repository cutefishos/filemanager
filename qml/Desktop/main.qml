import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI
import "../"

Item {
    id: rootItem

    DesktopSettings {
        id: settings
    }

    GlobalSettings {
        id: globalSettings
    }

    Loader {
        id: backgroundLoader
        anchors.fill: parent
        sourceComponent: settings.backgroundType === 0 ? wallpaper : background
    }

    Component {
        id: background

        Rectangle {
            anchors.fill: parent
            color: settings.backgroundColor
        }
    }

    Component {
        id: wallpaper

        Image {
            source: "file://" + settings.wallpaper
            sourceSize: Qt.size(width * Screen.devicePixelRatio,
                                height * Screen.devicePixelRatio)
            fillMode: Image.PreserveAspectCrop
            clip: true
            cache: false

            ColorOverlay {
                id: dimsWallpaper
                anchors.fill: parent
                source: parent
                color: "#000000"
                opacity: FishUI.Theme.darkMode && settings.dimsWallpaper ? 0.4 : 0.0

                Behavior on opacity {
                    NumberAnimation {
                        duration: 200
                    }
                }

            }
        }
    }

    FolderModel {
        id: dirModel
        url: desktopPath()
        isDesktop: true
        viewAdapter: viewAdapter
    }

    ItemViewAdapter {
        id: viewAdapter
        adapterView: _folderView
        adapterModel: dirModel
        adapterIconSize: 40
        adapterVisibleArea: Qt.rect(_folderView.contentX, _folderView.contentY,
                                    _folderView.contentWidth, _folderView.contentHeight)
    }

    FolderGridView {
        id: _folderView
        anchors.fill: parent

        isDesktopView: true
        iconSize: globalSettings.desktopIconSize
        maximumIconSize: globalSettings.maximumIconSize
        minimumIconSize: globalSettings.minimumIconSize
        focus: true
        model: dirModel

        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

        onIconSizeChanged: {
            globalSettings.desktopIconSize = _folderView.iconSize
        }

        leftMargin: desktopView.screenAvailableRect ? desktopView.screenAvailableRect.x : 0
        topMargin: desktopView.screenAvailableRect ? desktopView.screenAvailableRect.y : 0
        rightMargin: desktopView.screenRect.width - (desktopView.screenAvailableRect.x + desktopView.screenAvailableRect.width)
        bottomMargin: desktopView.screenRect.height - (desktopView.screenAvailableRect.y + desktopView.screenAvailableRect.height)

        flow: GridView.FlowTopToBottom

        delegate: FolderGridItem {}

        onActiveFocusChanged: {
            if (!activeFocus)
                dirModel.clearSelection()
        }

        Component.onCompleted: {
            dirModel.requestRename.connect(rename)
        }
    }

    Connections {
        target: _folderView

        function onKeyPress(event) {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
                dirModel.openSelected()
            else if (event.key === Qt.Key_C && event.modifiers & Qt.ControlModifier)
                dirModel.copy()
            else if (event.key === Qt.Key_X && event.modifiers & Qt.ControlModifier)
                dirModel.cut()
            else if (event.key === Qt.Key_V && event.modifiers & Qt.ControlModifier)
                dirModel.paste()
            else if (event.key === Qt.Key_F2)
                dirModel.requestRename()
            else if (event.key === Qt.Key_A && event.modifiers & Qt.ControlModifier)
                dirModel.selectAll()
            else if (event.key === Qt.Key_Delete)
                dirModel.keyDeletePress()
        }
    }

    Component {
        id: rubberBandObject

        RubberBand {
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
