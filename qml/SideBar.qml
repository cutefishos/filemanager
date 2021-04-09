import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.0

import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

ListView {
    id: sideBar

    signal clicked(string path)

    PlacesModel {
        id: placesModel
    }

    model: placesModel
    clip: true

    leftMargin: FishUI.Units.smallSpacing
    rightMargin: FishUI.Units.smallSpacing
    bottomMargin: FishUI.Units.smallSpacing
    spacing: FishUI.Units.smallSpacing

    ScrollBar.vertical: ScrollBar {
        bottomPadding: FishUI.Units.smallSpacing
    }

    highlightFollowsCurrentItem: true
    highlightMoveDuration: 0
    highlightResizeDuration : 0

    highlight: Rectangle {
        radius: FishUI.Theme.smallRadius
        color: FishUI.Theme.highlightColor
    }

    delegate: Item {
        id: _item
        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
        height: 40

        property bool checked: sideBar.currentIndex === index
        property color hoveredColor: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.backgroundColor, 1.1)
                                                         : Qt.darker(FishUI.Theme.backgroundColor, 1.1)
        MouseArea {
            id: _mouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            onClicked: {
                if (model.isDevice && model.setupNeeded)
                    placesModel.requestSetup(index)

                // sideBar.currentIndex = index
                sideBar.clicked(model.path ? model.path : model.url)
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: FishUI.Theme.smallRadius
            color: _mouseArea.containsMouse && !checked ? _item.hoveredColor : "transparent"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: FishUI.Units.smallSpacing
            anchors.rightMargin: FishUI.Units.smallSpacing
            spacing: FishUI.Units.smallSpacing

            Image {
                height: _item.height * 0.55
                width: height
                sourceSize: Qt.size(width, height)
                source: model.iconPath ? model.iconPath : "image://icontheme/" + model.iconName
                Layout.alignment: Qt.AlignVCenter

                ColorOverlay {
                    anchors.fill: parent
                    source: parent
                    color: _label.color
                    visible: FishUI.Theme.darkMode && model.iconPath || checked
                }
            }

            Label {
                id: _label
                text: model.name
                color: checked ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    function updateSelection(path) {
        sideBar.currentIndex = -1

        for (var i = 0; i < sideBar.count; ++i) {
            if (path === sideBar.model.get(i).path ||
                    path === sideBar.model.get(i).url) {
                sideBar.currentIndex = i
                break
            }
        }
    }
}
