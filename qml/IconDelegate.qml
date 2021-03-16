import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.0

import MeuiKit 1.0 as Meui

import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    id: main

    property int index: model.index
    property string name: model.blank ? "" : model.display
    property bool blank: model.blank
    property bool isDir: model.blank ? false : model.isDir
    property bool selected: model.blank ? false : model.selected
    property Item frame: contentItem
    property Item iconArea: icon
    property Item labelArea: label

    property bool hovered: (main.GridView.view.hoveredItem === main)

    property color hoveredColor: Qt.rgba(Meui.Theme.highlightColor.r,
                                         Meui.Theme.highlightColor.g,
                                         Meui.Theme.highlightColor.b, 0.1)
    property color selectedColor: Qt.rgba(Meui.Theme.highlightColor.r,
                                         Meui.Theme.highlightColor.g,
                                         Meui.Theme.highlightColor.b, 0.9)

    Accessible.name: name
    Accessible.role: Accessible.Canvas

    onSelectedChanged: {
        if (selected && !blank) {
            contentItem.grabToImage(function(result) {
                dir.addItemDragImage(positioner.map(index), main.x + contentItem.x, main.y + contentItem.y, contentItem.width, contentItem.height, result.image);
            });
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: Meui.Units.largeSpacing
        radius: Meui.Theme.bigRadius
        color: selected ? selectedColor : main.hovered ? hoveredColor : "transparent"

        border.color: Qt.rgba(Meui.Theme.highlightColor.r,
                              Meui.Theme.highlightColor.g,
                              Meui.Theme.highlightColor.b, 0.3)
        border.width: main.hovered || selected ? 1 : 0
    }

    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: Meui.Units.largeSpacing

        PlasmaCore.IconItem {
            id: icon
            z: 2

            anchors.top: parent.top
            anchors.topMargin: Meui.Units.smallSpacing
            anchors.horizontalCenter: parent.horizontalCenter

            height: main.height * 0.55
            width: height

            animated: false
            usesPlasmaTheme: false
            smooth: true
            source: model.blank ? "" : model.decoration
            overlays: model.blank ? "" : model.overlays
        }

        Label {
            id: label
            z: 2

            anchors.top: icon.bottom
            anchors.topMargin: Meui.Units.smallSpacing
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            width: parent.width

            textFormat: Text.PlainText

            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignTop

            wrapMode: Text.Wrap
            elide: Text.ElideRight
            color: Meui.Theme.textColor
            opacity: model.isHidden ? 0.6 : 1
            text: model.blank ? "" : model.display
            font.italic: model.isLink
        }
    }
}
