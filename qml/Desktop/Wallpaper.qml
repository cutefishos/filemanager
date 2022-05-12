import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0 as FM
import FishUI 1.0 as FishUI

Item {
    id: control

    FM.DesktopSettings {
        id: settings
    }

    Loader {
        id: backgroundLoader
        anchors.fill: parent
        anchors.margins: 0
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

            // Clear cache
            onSourceChanged: dirModel.clearPixmapCache()

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
}
