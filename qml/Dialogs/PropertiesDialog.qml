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
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import FishUI 1.0 as FishUI

FishUI.Window {
    id: control

    property int widthValue: _mainLayout.implicitWidth + FishUI.Units.largeSpacing * 3
    property int heightValue: _mainLayout.implicitHeight + control.header.height + FishUI.Units.largeSpacing * 2

    title: qsTr("Properties")
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    minimizeButtonVisible: false
    visible: false

    width: widthValue
    height: heightValue
    minimumWidth: widthValue
    minimumHeight: heightValue
    maximumWidth: widthValue
    maximumHeight: heightValue

    Shortcut {
        sequence: "Escape"
        onActivated: control.close()
    }

    background.color: FishUI.Theme.secondBackgroundColor

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.largeSpacing * 1.5
        anchors.rightMargin: FishUI.Units.largeSpacing * 1.5
        anchors.topMargin: FishUI.Units.smallSpacing
        anchors.bottomMargin: FishUI.Units.largeSpacing * 1.5
        spacing: FishUI.Units.largeSpacing

        RowLayout {
            spacing: FishUI.Units.largeSpacing * 2

            FishUI.IconItem {
                width: 48
                height: 48
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                source: main.iconName

                // The source is available while the item is being created.
                // Refresh after completion so IconItem can load it once its
                // component lifecycle is ready.
                Component.onCompleted: updateIcon()
            }

            FishUI.Label {
                text: main.fileName
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }
        }

        GridLayout {
            columns: 2
            Layout.fillWidth: true
            columnSpacing: FishUI.Units.largeSpacing
            rowSpacing: FishUI.Units.largeSpacing
            Layout.alignment: Qt.AlignTop

            FishUI.Label {
                text: qsTr("Type:")
                Layout.alignment: Qt.AlignRight
                color: FishUI.Theme.disabledTextColor
                visible: mimeType.visible
            }

            FishUI.Label {
                id: mimeType
                text: main.mimeType
                visible: text
            }

            FishUI.Label {
                text: qsTr("Location:")
                Layout.alignment: Qt.AlignRight
                color: FishUI.Theme.disabledTextColor
            }

            FishUI.Label {
                id: location
                text: main.location
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            FishUI.Label {
                text: qsTr("Size:")
                Layout.alignment: Qt.AlignRight
                color: FishUI.Theme.disabledTextColor
            }

            FishUI.Label {
                id: size
                text: main.fileSize ? main.fileSize : qsTr("Calculating...")
            }

            FishUI.Label {
                text: qsTr("Created:")
                Layout.alignment: Qt.AlignRight
                color: FishUI.Theme.disabledTextColor
                visible: creationTime.visible
            }

            FishUI.Label {
                id: creationTime
                text: main.creationTime
                visible: text
            }

            FishUI.Label {
                text: qsTr("Modified:")
                Layout.alignment: Qt.AlignRight
                color: FishUI.Theme.disabledTextColor
                visible: modifiedTime.visible
            }

            FishUI.Label {
                id: modifiedTime
                text: main.modifiedTime
                visible: text
            }

            FishUI.Label {
                text: qsTr("Accessed:")
                Layout.alignment: Qt.AlignRight
                color: FishUI.Theme.disabledTextColor
                visible: accessTime.visible
            }

            FishUI.Label {
                id: accessTime
                text: main.accessedTime
                visible: text
            }
        }

    }
}
