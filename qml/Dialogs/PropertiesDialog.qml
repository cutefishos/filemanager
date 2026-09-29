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

    // The FileProperties backend describing the selected items.
    required property QtObject details

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
    // The background item reaches a grown window a frame or two after its buffer does.
    color: background.color

    ColumnLayout {
        id: _mainLayout
        // Not filled to the window: the window height follows this layout's implicit
        // height a polish later, and a filled layout would be spread over the old height.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: FishUI.Units.largeSpacing * 1.5
        anchors.rightMargin: FishUI.Units.largeSpacing * 1.5
        anchors.topMargin: FishUI.Units.smallSpacing
        spacing: FishUI.Units.largeSpacing

        // The icon ends where the labels below do and the name starts where their values do.
        RowLayout {
            spacing: _form.columnSpacing

            Item {
                Layout.preferredWidth: _form.labelWidth
                Layout.preferredHeight: 48

                FishUI.IconItem {
                    anchors.right: parent.right
                    width: 48
                    height: 48
                    source: control.details.iconName

                    // The source is available while the item is being created.
                    // Refresh after completion so IconItem can load it once its
                    // component lifecycle is ready.
                    Component.onCompleted: updateIcon()
                }
            }

            FishUI.Label {
                text: control.details.fileName
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }
        }

        GridLayout {
            id: _form
            columns: 2
            Layout.fillWidth: true
            columnSpacing: FishUI.Units.largeSpacing
            rowSpacing: FishUI.Units.largeSpacing
            Layout.alignment: Qt.AlignTop

            // Hidden rows leave the grid, so the values would start further left for some files.
            readonly property real labelWidth: Math.max(48,
                                                        _typeLabel.implicitWidth,
                                                        _locationLabel.implicitWidth,
                                                        _sizeLabel.implicitWidth,
                                                        _createdLabel.implicitWidth,
                                                        _modifiedLabel.implicitWidth,
                                                        _accessedLabel.implicitWidth)

            FishUI.Label {
                id: _typeLabel
                text: qsTr("Type:")
                Layout.preferredWidth: _form.labelWidth
                Layout.alignment: Qt.AlignRight
                horizontalAlignment: Text.AlignRight
                color: FishUI.Theme.disabledTextColor
                visible: mimeType.visible
            }

            FishUI.Label {
                id: mimeType
                text: control.details.mimeType
                visible: text
            }

            FishUI.Label {
                id: _locationLabel
                text: qsTr("Location:")
                Layout.preferredWidth: _form.labelWidth
                Layout.alignment: Qt.AlignRight
                horizontalAlignment: Text.AlignRight
                color: FishUI.Theme.disabledTextColor
            }

            FishUI.Label {
                id: location
                text: control.details.location
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            FishUI.Label {
                id: _sizeLabel
                text: qsTr("Size:")
                Layout.preferredWidth: _form.labelWidth
                Layout.alignment: Qt.AlignRight
                horizontalAlignment: Text.AlignRight
                color: FishUI.Theme.disabledTextColor
            }

            FishUI.Label {
                id: size
                text: control.details.fileSize ? control.details.fileSize : qsTr("Calculating...")
            }

            FishUI.Label {
                id: _createdLabel
                text: qsTr("Created:")
                Layout.preferredWidth: _form.labelWidth
                Layout.alignment: Qt.AlignRight
                horizontalAlignment: Text.AlignRight
                color: FishUI.Theme.disabledTextColor
                visible: creationTime.visible
            }

            FishUI.Label {
                id: creationTime
                text: control.details.creationTime
                visible: text
            }

            FishUI.Label {
                id: _modifiedLabel
                text: qsTr("Modified:")
                Layout.preferredWidth: _form.labelWidth
                Layout.alignment: Qt.AlignRight
                horizontalAlignment: Text.AlignRight
                color: FishUI.Theme.disabledTextColor
                visible: modifiedTime.visible
            }

            FishUI.Label {
                id: modifiedTime
                text: control.details.modifiedTime
                visible: text
            }

            FishUI.Label {
                id: _accessedLabel
                text: qsTr("Accessed:")
                Layout.preferredWidth: _form.labelWidth
                Layout.alignment: Qt.AlignRight
                horizontalAlignment: Text.AlignRight
                color: FishUI.Theme.disabledTextColor
                visible: accessTime.visible
            }

            FishUI.Label {
                id: accessTime
                text: control.details.accessedTime
                visible: text
            }
        }

    }
}
