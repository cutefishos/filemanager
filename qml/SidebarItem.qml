import QtQuick 2.4
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0

import MeuiKit 1.0 as Meui

Item {
    id: item

    property bool checked: false
    signal clicked

    Rectangle {
        id: rect
        anchors.fill: parent
        radius: Meui.Theme.smallRadius
        color: item.checked ? "transparent"
                           : mouseArea.containsMouse ? Qt.rgba(Meui.Theme.textColor.r,
                                                               Meui.Theme.textColor.g,
                                                               Meui.Theme.textColor.b, 0.1) : "transparent"
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: item.clicked()
    }

    RowLayout {
        anchors.fill: rect
        anchors.leftMargin: Meui.Units.largeSpacing
        anchors.rightMargin: Meui.Units.largeSpacing

        spacing: Meui.Units.largeSpacing

        Item {
            id: iconItem
            height: item.height * 0.55
            width: height

            Image {
                id: image
                anchors.fill: parent
                sourceSize: Qt.size(width, height)
                source: model.iconPath ? model.iconPath : "image://icontheme/" + model.iconName
                Layout.alignment: Qt.AlignVCenter
            }

            ColorOverlay {
                anchors.fill: parent
                source: parent
                color: itemTitle.color
                visible: Meui.Theme.darkMode && model.iconPath || checked
            }
        }

        Label {
            id: itemTitle
            text: model.name
            color: item.checked ? Meui.Theme.highlightedTextColor : Meui.Theme.textColor
            elide: Text.ElideRight
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
        }
    }
}
