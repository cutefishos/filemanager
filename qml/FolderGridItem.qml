import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui

Item {
    id: control

    width: GridView.view.cellWidth
    height: GridView.view.cellHeight

    property Item iconArea: _image.visible ? _image : _icon
    property Item labelArea: _label
    property Item background: _background

    property int index: model.index
    property bool hovered: GridView.view.hoveredItem === control
    property bool selected: model.selected
    property bool blank: model.blank
    property var fileName: model.fileName

    Rectangle {
        id: _background
        width: Math.max(_iconItem.width, _label.paintedWidth)
        height: _iconItem.height + _label.paintedHeight + Meui.Units.largeSpacing
        x: (parent.width - width) / 2
        y: _iconItem.y
        color: selected || hovered ? Meui.Theme.highlightColor : "transparent"
        radius: Meui.Theme.mediumRadius
        visible: selected || hovered
        opacity: selected ? 1.0 : 0.4
    }

    Item {
        id: _iconItem
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: Meui.Units.largeSpacing
        anchors.bottomMargin: Meui.Units.largeSpacing
        z: 2

        width: parent.width - Meui.Units.largeSpacing * 2
        height: control.GridView.view.iconSize

        Image {
            id: _icon
            width: control.GridView.view.iconSize
            height: control.GridView.view.iconSize
            anchors.centerIn: parent
            sourceSize: Qt.size(width, height)
            source: "image://icontheme/" + model.iconName
            visible: !_image.visible
        }

        Image {
            id: _image
            anchors.fill: parent
            anchors.topMargin: Meui.Units.smallSpacing
            anchors.leftMargin: Meui.Units.smallSpacing
            anchors.rightMargin: Meui.Units.smallSpacing
            fillMode: Image.PreserveAspectFit
            visible: status === Image.Ready
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            sourceSize.width: width
            sourceSize.height: height
            source: model.thumbnail ? model.thumbnail : ""
            asynchronous: true
            cache: true

            layer.enabled: true
            layer.effect: OpacityMask {
                maskSource: Item {
                    width: _image.width
                    height: _image.height

                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, _image.paintedWidth)
                        height: Math.min(parent.height, _image.paintedHeight)
                        radius: Meui.Theme.smallRadius / 2
                    }
                }
            }
        }
    }

    Label {
        id: _label
        z: 2
        anchors.top: _iconItem.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: Meui.Units.smallSpacing
        maximumLineCount: 2
        horizontalAlignment: Text.AlignHCenter
        width: parent.width - Meui.Units.largeSpacing * 2 - Meui.Units.smallSpacing
        textFormat: Text.PlainText
        elide: Qt.ElideRight
        wrapMode: Text.Wrap
        text: model.fileName
        color: control.GridView.view.isDesktopView ? "white"
                                                   : selected ? Meui.Theme.highlightedTextColor : Meui.Theme.textColor
    }

    DropShadow {
        anchors.fill: _label
        source: _label
        z: 1
        horizontalOffset: 1
        verticalOffset: 1
        radius: Math.round(4 * Meui.Units.devicePixelRatio)
        samples: radius * 2 + 1
        spread: 0.35
        color: Qt.rgba(0, 0, 0, 0.3)
        opacity: model.isHidden ? 0.6 : 1
        visible: control.GridView.view.isDesktopView
    }
}
