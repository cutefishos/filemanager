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
import FishUI 1.0 as FishUI

FishUI.DesktopMenu {
    id: control

    MenuItem {
        Layout.fillWidth: true

        Image {
            id: _gridIcon
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: FishUI.Units.largeSpacing
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/grid.svg" : "qrc:/images/light/grid.svg"
            width: 22
            height: width
        }

        Text {
            anchors.left: _gridIcon.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: FishUI.Units.largeSpacing
            text: qsTr("Icons")
            color: FishUI.Theme.textColor
        }

        Image {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: FishUI.Units.largeSpacing * 1.5
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/checked.svg" : "qrc:/images/light/checked.svg"
            width: 22
            height: width
            visible: settings.viewMethod === 1
        }

        onTriggered: settings.viewMethod = 1
    }

    MenuItem {
        Layout.fillWidth: true

        Image {
            id: _listIcon
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: FishUI.Units.largeSpacing
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/list.svg" : "qrc:/images/light/list.svg"
            width: 22
            height: width
        }

        Text {
            anchors.left: _listIcon.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: FishUI.Units.largeSpacing
            text: qsTr("List")
            color: FishUI.Theme.textColor
        }

        Image {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: FishUI.Units.largeSpacing * 1.5
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/checked.svg" : "qrc:/images/light/checked.svg"
            width: 22
            height: width
            visible: settings.viewMethod === 0
        }

        onTriggered: settings.viewMethod = 0
    }

    // MenuSeparator {}

    MenuItem {
        Layout.fillWidth: true

        Image {
            id: orderByNameIcon
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: FishUI.Units.largeSpacing
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/order_by_name.svg" : "qrc:/images/light/order_by_name.svg"
            width: 22
            height: width
        }

        Text {
            anchors.left: orderByNameIcon.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: FishUI.Units.largeSpacing
            text: qsTr("Name")
            color: FishUI.Theme.textColor
        }

        Image {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: FishUI.Units.largeSpacing * 1.5
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/up.svg" : "qrc:/images/light/up.svg"
            height: width
            width: 22
            visible: settings.orderBy === 0
        }

        onTriggered: settings.orderBy = 0
    }

    MenuItem {
        Layout.fillWidth: true

        Image {
            id: orderByDateIcon
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: FishUI.Units.largeSpacing
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/date.svg" : "qrc:/images/light/date.svg"
            width: 22
            height: width
        }

        Text {
            anchors.left: orderByDateIcon.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: FishUI.Units.largeSpacing
            text: qsTr("Date")
            color: FishUI.Theme.textColor
        }

        Image {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: FishUI.Units.largeSpacing * 1.5
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/up.svg" : "qrc:/images/light/up.svg"
            width: 22
            height: width
            visible: settings.orderBy === 1
        }

        onTriggered: settings.orderBy = 1
    }

    MenuItem {
        Layout.fillWidth: true

        Image {
            id: orderBySizeIcon
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: FishUI.Units.largeSpacing
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/size.svg" : "qrc:/images/light/size.svg"
            width: 22
            height: width
        }

        Text {
            anchors.left: orderBySizeIcon.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: FishUI.Units.largeSpacing
            text: qsTr("Size")
            color: FishUI.Theme.textColor
        }

        Image {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: FishUI.Units.largeSpacing * 1.5
            source: FishUI.Theme.darkMode ? "qrc:/images/dark/up.svg" : "qrc:/images/light/up.svg"
            width: 22
            height: width
            visible: settings.orderBy === 2
        }

        onTriggered: settings.orderBy = 2
    }
}
