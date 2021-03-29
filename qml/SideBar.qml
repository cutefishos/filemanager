import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.0

import MeuiKit 1.0 as Meui
import Cutefish.FileManager 1.0

ListView {
    id: sideBar

    signal clicked(string path)

    PlacesModel {
        id: placesModel
    }

    model: placesModel
    clip: true

    leftMargin: Meui.Units.smallSpacing
    rightMargin: Meui.Units.smallSpacing
    spacing: Meui.Units.largeSpacing

    ScrollBar.vertical: ScrollBar {}

    highlightFollowsCurrentItem: true
    highlightMoveDuration: 0
    highlightResizeDuration : 0

    highlight: Rectangle {
        radius: Meui.Theme.smallRadius
        color: Meui.Theme.highlightColor
    }

    delegate: Item {
        id: _item
        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
        height: 40

        property bool checked: sideBar.currentIndex === index
        property color hoveredColor: Meui.Theme.darkMode ? Qt.lighter(Meui.Theme.backgroundColor, 1.1)
                                                         : Qt.darker(Meui.Theme.backgroundColor, 1.1)
        MouseArea {
            id: _mouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            onClicked: {
                sideBar.currentIndex = index
                sideBar.clicked(model.path)
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: Meui.Theme.smallRadius
            color: _mouseArea.containsMouse && !checked ? _item.hoveredColor : "transparent"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Meui.Units.smallSpacing
            anchors.rightMargin: Meui.Units.smallSpacing
            spacing: Meui.Units.smallSpacing

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
                    visible: Meui.Theme.darkMode && model.iconPath || checked
                }
            }

            Label {
                id: _label
                text: model.name
                color: checked ? Meui.Theme.highlightedTextColor : Meui.Theme.textColor
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    function updateSelection(path) {
        sideBar.currentIndex = -1

        for (var i = 0; i < sideBar.count; ++i) {
            if (path === sideBar.model.get(i).path) {
                sideBar.currentIndex = i
                break
            }
        }
    }
}
