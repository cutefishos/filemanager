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

    FolderGridView {
        anchors.fill: parent
        cellHeight: globalSettings.desktopIconSize
        cellWidth: globalSettings.desktopIconSize
        model: FolderModel {
            id: folderModel
            url: desktopPath()
            isDesktop: true
        }

        leftMargin: desktopView.screenAvailableRect ? desktopView.screenAvailableRect.x : 0
        topMargin: desktopView.screenAvailableRect ? desktopView.screenAvailableRect.y : 0
        rightMargin: desktopView.screenRect.width - (desktopView.screenAvailableRect.x + desktopView.screenAvailableRect.width)
        bottomMargin: desktopView.screenRect.height - (desktopView.screenAvailableRect.y + desktopView.screenAvailableRect.height)

        delegate: FolderGridItem {}
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
