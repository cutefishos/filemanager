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
import FishUI 1.0 as FishUI

Item {
    id: control
    width: 24
    height: 24

    property alias source: _image.source
    // A FishUI.Icon glyph drawn instead of source.
    property alias iconName: _icon.name
    property int iconSize: 18
    // Translucent, so the buttons sit on the frosted header without a solid
    // patch. Over the plain window background they match the old opaque tones.
    property color backgroundColor: FishUI.Theme.darkMode ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(1, 1, 1, 0.6)
    property color hoveredColor: Qt.rgba(FishUI.Theme.textColor.r, FishUI.Theme.textColor.g,
                                         FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.21 : 0.17)
    property color pressedColor: Qt.rgba(FishUI.Theme.textColor.r, FishUI.Theme.textColor.g,
                                         FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.15 : 0.26)
    property bool highlighted: false

    signal clicked()

    Rectangle {
        id: _background
        anchors.fill: parent
        // Same inset as FishUI's window buttons, so both draw the same circle.
        anchors.margins: Math.round(control.width * 0.1)
        radius: width / 2
        color: !control.enabled ? "transparent"
             : _mouseArea.pressed ? pressedColor
             : _mouseArea.containsMouse || control.highlighted ? control.hoveredColor
             : control.backgroundColor
    }

    Image {
        id: _image
        anchors.centerIn: parent
        width: control.iconSize
        height: width
        sourceSize: Qt.size(width, height)
        smooth: false
        antialiasing: true
        opacity: control.enabled ? 1 : 0.35
        visible: !control.iconName
    }

    FishUI.Icon {
        id: _icon
        anchors.centerIn: parent
        width: control.iconSize
        height: width
        opacity: control.enabled ? 1 : 0.35
        visible: !!control.iconName
    }

    MouseArea {
        id: _mouseArea
        anchors.fill: parent
        enabled: control.enabled
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: control.clicked()
    }
}
