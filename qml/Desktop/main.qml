import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui
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
            sourceSize: Qt.size(width, height)
            fillMode: Image.PreserveAspectCrop
            clip: true
            cache: false

            ColorOverlay {
                id: dimsWallpaper
                anchors.fill: parent
                source: parent
                color: "#000000"
                opacity: Meui.Theme.darkMode && settings.dimsWallpaper ? 0.4 : 0.0

                Behavior on opacity {
                    NumberAnimation {
                        duration: 200
                    }
                }

            }
        }
    }

    FolderModel {
        id: folderModel
        url: desktopPath()
        isDesktop: true
    }

    FolderGridView {
        id: _folderView
        anchors.fill: parent

        isDesktopView: true
        iconSize: globalSettings.desktopIconSize
        maximumIconSize: globalSettings.maximumIconSize
        minimumIconSize: globalSettings.minimumIconSize
        focus: true
        model: folderModel

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
                folderModel.clearSelection()
        }
    }

    Connections {
        target: _folderView

        function onKeyPress(event) {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
                folderModel.openSelected()
            else if (event.key === Qt.Key_C && event.modifiers & Qt.ControlModifier)
                folderModel.copy()
            else if (event.key === Qt.Key_X && event.modifiers & Qt.ControlModifier)
                folderModel.cut()
            else if (event.key === Qt.Key_V && event.modifiers & Qt.ControlModifier)
                folderModel.paste()
            else if (event.key === Qt.Key_F2)
                folderModel.requestRename()
            else if (event.key === Qt.Key_L && event.modifiers & Qt.ControlModifier)
                folderPage.requestPathEditor()
            else if (event.key === Qt.Key_A && event.modifiers & Qt.ControlModifier)
                folderModel.selectAll()
            else if (event.key === Qt.Key_Backspace)
                folderModel.up()
        }
    }

    Component {
        id: rubberBandObject

        RubberBand {
            id: rubberBand

            width: 0
            height: 0
            z: 99999
            color: Meui.Theme.highlightColor

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
