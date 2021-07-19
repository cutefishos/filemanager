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

Window {
    id: control
    title: qsTr("Properties")
    flags: Qt.Dialog

    visible: true

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.secondBackgroundColor
    }

    onVisibleChanged: {
        if (visible) updateWindowSize()
    }

    function updateWindowSize() {
        if (visible) {
            control.width = _mainLayout.implicitWidth + _mainLayout.anchors.leftMargin + _mainLayout.anchors.rightMargin
            control.height = _mainLayout.implicitHeight + _mainLayout.anchors.topMargin + _mainLayout.anchors.bottomMargin
            control.minimumWidth = control.width
            control.minimumHeight = control.height
            control.maximumWidth = control.width
            control.maximumHeight = control.height

            if (_textField.enabled)
                _textField.forceActiveFocus()
        }
    }

    Item {
        id: _contentItem
        anchors.fill: parent
        focus: true

        Keys.enabled: true
        Keys.onEscapePressed: control.close()

        ColumnLayout {
            id: _mainLayout
            anchors.fill: parent
            anchors.leftMargin: FishUI.Units.largeSpacing * 2
            anchors.rightMargin: FishUI.Units.largeSpacing * 2
            anchors.topMargin: FishUI.Units.largeSpacing
            anchors.bottomMargin: FishUI.Units.largeSpacing * 1.5
            spacing: FishUI.Units.largeSpacing

            RowLayout {
                spacing: FishUI.Units.largeSpacing * 2

                Image {
                    width: 64
                    height: width
                    sourceSize: Qt.size(width, height)
                    source: "image://icontheme/" + main.iconName
                }

                TextField {
                    id: _textField
                    text: main.fileName
                    focus: true
                    Layout.fillWidth: true
                    Keys.onEscapePressed: control.close()
                    enabled: !main.multiple && main.isWritable
                }
            }

            GridLayout {
                columns: 2
                columnSpacing: FishUI.Units.largeSpacing
                rowSpacing: FishUI.Units.largeSpacing
                Layout.alignment: Qt.AlignTop

                onHeightChanged: updateWindowSize()
                onImplicitHeightChanged: updateWindowSize()

                Label {
                    text: qsTr("Type:")
                    Layout.alignment: Qt.AlignRight
                    color: FishUI.Theme.disabledTextColor
                    visible: mimeType.visible
                }

                Label {
                    id: mimeType
                    text: main.mimeType
                    visible: text
                }

                Label {
                    text: qsTr("Location:")
                    Layout.alignment: Qt.AlignRight
                    color: FishUI.Theme.disabledTextColor
                }

                Label {
                    id: location
                    text: main.location
                }

                Label {
                    text: qsTr("Size:")
                    Layout.alignment: Qt.AlignRight
                    color: FishUI.Theme.disabledTextColor
                    // visible: size.visible
                }

                Label {
                    id: size
                    text: main.size ? main.size : qsTr("Calculating...")
                    // visible: text
                }

                Label {
                    text: qsTr("Created:")
                    Layout.alignment: Qt.AlignRight
                    color: FishUI.Theme.disabledTextColor
                    visible: creationTime.visible
                }

                Label {
                    id: creationTime
                    text: main.creationTime
                    visible: text
                }

                Label {
                    text: qsTr("Modified:")
                    Layout.alignment: Qt.AlignRight
                    color: FishUI.Theme.disabledTextColor
                    visible: modifiedTime.visible
                }

                Label {
                    id: modifiedTime
                    text: main.modifiedTime
                    visible: text
                }

                Label {
                    text: qsTr("Accessed:")
                    Layout.alignment: Qt.AlignRight
                    color: FishUI.Theme.disabledTextColor
                    visible: accessTime.visible
                }

                Label {
                    id: accessTime
                    text: main.accessedTime
                    visible: text
                }
            }

            Item {
                height: FishUI.Units.largeSpacing
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: FishUI.Units.largeSpacing

                Button {
                    text: qsTr("Cancel")
                    Layout.fillWidth: true
                    onClicked: {
                        control.close()
                        main.reject()
                    }
                }

                Button {
                    text: qsTr("OK")
                    Layout.fillWidth: true
                    onClicked: {
                        main.accept(_textField.text)
                        control.close()
                    }
                    flat: true
                }
            }
        }
    }
}
