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
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI

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

    // For desktop
    visible: GridView.view.isDesktopView ? !blank : true

    onSelectedChanged: {
        if (!GridView.view.isDesktopView)
            return

        if (selected && !blank) {
            control.grabToImage(function(result) {
                dirModel.addItemDragImage(control.index,
                                          control.x,
                                          control.y,
                                          control.width,
                                          control.height,
                                          result.image)
            })
        }
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
        anchors.topMargin: FishUI.Units.largeSpacing
        anchors.bottomMargin: FishUI.Units.largeSpacing
        z: 2

        width: parent.width - FishUI.Units.largeSpacing * 2
        height: control.GridView.view.iconSize

        Image {
            id: _icon
            width: control.GridView.view.iconSize
            height: control.GridView.view.iconSize
            anchors.centerIn: parent
            sourceSize: Qt.size(width, height)
            source: "image://icontheme/" + model.iconName
            visible: !_image.visible
            smooth: false

            ColorOverlay {
                anchors.fill: _icon
                source: _icon
                color: FishUI.Theme.highlightColor
                opacity: 0.5
                visible: control.selected
            }

            ColorOverlay {
                anchors.fill: _icon
                source: _icon
                color: "white"
                opacity: FishUI.Theme.darkMode ? 0.3 : 0.4
                visible: control.hovered && !control.selected
            }
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
            sourceSize.width: width
            sourceSize.height: height
            source: model.thumbnail ? model.thumbnail : ""
            asynchronous: true
            cache: false
            smooth: false

            // Because of the effect of OpacityMask.
            ColorOverlay {
                anchors.fill: _image
                source: _image
                color: FishUI.Theme.highlightColor
                opacity: 0.5
                visible: control.selected
            }

            ColorOverlay {
                anchors.fill: _image
                source: _image
                color: "white"
                opacity: FishUI.Theme.darkMode ? 0.3 : 0.4
                visible: control.hovered && !control.selected
            }

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

//        ColorOverlay {
//            id: _selectedColorOverlay
//            anchors.fill: _image.visible ? _image : _icon
//            source: _image.visible ? _image : _icon
//            color: FishUI.Theme.highlightColor
//            opacity: 0.5
//            visible: control.selected
//        }

//        ColorOverlay {
//            id: _hoveredColorOverlay
//            anchors.fill: _image.visible ? _image : _icon
//            source: _image.visible ? _image : _icon
//            color: Qt.lighter(FishUI.Theme.highlightColor, 1.6)
//            opacity: FishUI.Theme.darkMode ? 0.4 : 0.6
//            visible: control.hovered && !control.selected
//        }
    }

    Label {
        id: _label
        z: 2
        anchors.top: _iconItem.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: FishUI.Units.smallSpacing
        maximumLineCount: 2
        horizontalAlignment: Text.AlignHCenter
        width: parent.width - FishUI.Units.largeSpacing * 2 - FishUI.Units.smallSpacing
        textFormat: Text.PlainText
        elide: Qt.ElideRight
        wrapMode: Text.Wrap
        text: model.fileName
        color: control.GridView.view.isDesktopView ? "white"
                                                   : selected ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
    }

    Rectangle {
        z: 1
        x: _label.x
        y: _label.y
        width: _label.width
        height: _label.height
        radius: 4
        color: FishUI.Theme.highlightColor
        opacity: control.selected ? 1.0 : 0
    }

    DropShadow {
        anchors.fill: _label
        source: _label
        z: 1
        horizontalOffset: 1
        verticalOffset: 1
        radius: Math.round(4 * FishUI.Units.devicePixelRatio)
        samples: radius * 2 + 1
        spread: 0.35
        color: Qt.rgba(0, 0, 0, 0.3)
        opacity: model.isHidden ? 0.6 : 1
        visible: control.GridView.view.isDesktopView && !control.selected
    }
}
