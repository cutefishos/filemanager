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
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import Qt5Compat.GraphicalEffects

import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI

Item {
    id: control

    width: GridView.view.cellWidth
    height: GridView.view.cellHeight

    property Item iconArea: _image.visible ? _image : _icon
    property string editName: model.fileName
    property Item labelArea: _label
    property Item background: _background

    readonly property bool desktop: GridView.view.isDesktopView

    property int index: model.index
    property bool hovered: GridView.view.hoveredItem === control
    property bool selected: model.selected
    property bool blank: model.blank
    property var fileName: model.displayName
    readonly property color selectedTextColor: {
        const color = FishUI.Theme.highlightColor
        const luminance = color.r * 0.299 + color.g * 0.587 + color.b * 0.114
        return luminance > 0.6 ? "#323238" : "white"
    }

    // For desktop
    visible: GridView.view.isDesktopView ? !blank : true

    onSelectedChanged: updateDragImage()
    // blank and selected do not settle in the same order, so both have to ask.
    onBlankChanged: updateDragImage()

    // The drag pixmap is built from these snapshots, so one taken at the icon's
    // old cell would drag the wrong picture at the wrong offset from the cursor.
    onXChanged: updateDragImage()
    onYChanged: updateDragImage()
    onWidthChanged: updateDragImage()
    onHeightChanged: updateDragImage()
    Component.onCompleted: updateDragImage()

    function updateDragImage() {
        if (!GridView.view || !selected || blank || control.width <= 0 || control.height <= 0)
            return

        // Grabbing every icon a rubber band sweeps over stalls the band; the view
        // snapshots the final selection when the band closes.
        if (GridView.view.rubberBand)
            return

        var row = GridView.view.positioner.map(control.index)
        var x = control.x
        var y = control.y
        var width = control.width
        var height = control.height

        control.grabToImage(function(result) {
            if (control.GridView.view && control.selected && !control.blank
                    && control.GridView.view.positioner.map(control.index) === row)
                dirModel.addItemDragImage(row, x, y, width, height, result.image)
        })
    }

    Rectangle {
        id: _background
        width: Math.max(_iconItem.width, _label.paintedWidth)
        height: _iconItem.height + _label.paintedHeight + FishUI.Units.largeSpacing
        x: (parent.width - width) / 2
        y: _iconItem.y

        // (Deprecated) Rectangle rounded corner.
        //color: selected || hovered ? FishUI.Theme.highlightColor : "transparent"
        color: "transparent"
        // radius: FishUI.Theme.mediumRadius
        // visible: selected || hovered
        // opacity: selected ? 1.0 : 0.2
    }

    Item {
        id: _iconItem
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: control.desktop ? control.GridView.view.desktopIconTopMargin : FishUI.Units.smallSpacing
        anchors.bottomMargin: FishUI.Units.smallSpacing
        z: 2

        width: parent.width - FishUI.Units.largeSpacing * 2
        height: control.GridView.view.iconSize

        opacity: model.isHidden ? 0.5 : 1.0

        Image {
            id: _icon
            width: control.GridView.view.iconSize
            height: control.GridView.view.iconSize
            anchors.centerIn: parent
            sourceSize: Qt.size(width, height)
            source: "image://icontheme/" + model.iconName
            visible: !_image.visible
            smooth: true
            antialiasing: true
        }

        Image {
            id: _image
            anchors.fill: parent
            anchors.topMargin: FishUI.Units.smallSpacing
            anchors.leftMargin: FishUI.Units.smallSpacing
            anchors.rightMargin: FishUI.Units.smallSpacing
            fillMode: Image.PreserveAspectFit
            visible: status === Image.Ready
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            sourceSize: Qt.size(width, height)
            source: model.thumbnail ? model.thumbnail : ""
            asynchronous: true
            cache: false
            smooth: true
            antialiasing: true

            layer.enabled: _image.visible
            layer.effect: OpacityMask {
                maskSource: Item {
                    width: _image.width
                    height: _image.height

                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, _image.paintedWidth)
                        height: Math.min(parent.height, _image.paintedHeight)
                        radius: height * 0.1
                    }
                }
            }
        }

        Image {
            anchors.right: _icon.visible ? _icon.right : _image.right
            anchors.bottom: _icon.visible ? _icon.bottom : _image.bottom
            source: "image://icontheme/emblem-symbolic-link"
            width: control.desktop ? Math.max(16, Math.round(control.GridView.view.iconSize / 4)) : 16
            height: width
            visible: model.isLink
            sourceSize: Qt.size(width, height)
        }

        ColorOverlay {
            id: _selectedColorOverlay
            anchors.fill: _iconItem
            source: _iconItem
            color: FishUI.Theme.highlightColor
            opacity: 0.5
            visible: control.selected
        }

        ColorOverlay {
            id: _hoveredColorOverlay
            anchors.fill: _iconItem
            source: _iconItem
            color: "white"
            opacity: 0.1
            visible: control.hovered && !control.selected
        }
    }

    Label {
        id: _label
        z: 2
        anchors.top: _iconItem.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: control.desktop ? control.GridView.view.desktopLabelSpacing : FishUI.Units.smallSpacing
        maximumLineCount: control.desktop ? 2 : (control.selected ? 3 : 2)
        font: control.desktop ? control.GridView.view.desktopLabelFont : FishUI.Theme.defaultFont
        horizontalAlignment: Text.AlignHCenter
        width: control.desktop ? parent.width - control.GridView.view.desktopLabelInset * 2
                               : parent.width - FishUI.Units.largeSpacing * 2 - FishUI.Units.smallSpacing
        textFormat: Text.PlainText
        elide: Qt.ElideRight
        wrapMode: Text.Wrap
        text: control.fileName
        color: selected ? control.selectedTextColor
                        : control.GridView.view.isDesktopView ? "white" : FishUI.Theme.textColor
        opacity: model.isHidden ? 0.8 : 1.0
        // A layer effect on the label itself would lose hinting and fringe the
        // glyphs; the faint 1px edge hides the subpixel fringes instead.
        style: control.desktop && !control.selected ? Text.Raised : Text.Normal
        styleColor: Qt.rgba(0, 0, 0, 0.2)
    }

    Rectangle {
        readonly property real hPadding: control.desktop ? 4 : FishUI.Units.smallSpacing / 2
        readonly property real vPadding: control.desktop ? 1 : 0

        z: 1
        x: _label.x + (_label.width - _label.paintedWidth) / 2 - hPadding
        y: _label.y - vPadding
        visible: _label.visible
        width: _label.paintedWidth + hPadding * 2
        height: _label.paintedHeight + vPadding * 2
        radius: 4
        color: FishUI.Theme.highlightColor

        opacity: control.selected ? 1 : 0
    }

    // Soft halo from a blurred black copy; only its alpha survives the blur.
    Item {
        z: 1
        x: _label.x - 8
        y: _label.y - 8 + 1
        width: _label.width + 16
        height: _label.height + 16
        visible: control.desktop && _label.visible && !control.selected
        opacity: model.isHidden ? 0.25 : 0.45

        Text {
            x: 8
            y: 8
            width: _label.width
            text: _label.text
            font: _label.font
            textFormat: _label.textFormat
            horizontalAlignment: _label.horizontalAlignment
            wrapMode: _label.wrapMode
            maximumLineCount: _label.maximumLineCount
            elide: _label.elide
            color: "black"
        }

        layer.enabled: visible
        layer.effect: FastBlur {
            radius: 8
        }
    }
}
