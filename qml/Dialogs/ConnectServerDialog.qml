// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import FishUI 1.0 as FishUI

FishUI.DialogWindow {
    id: control

    property var placesModel

    title: qsTr("Connect to Server")
    width: 420
    contentHeight: layout.implicitHeight + 32
    background.color: FishUI.Theme.secondBackgroundColor
    // The background item reaches a grown window a frame or two after its buffer does.
    color: background.color

    function connectTo(address) {
        if (address.trim().length === 0)
            return
        control.close()
        control.placesModel.connectToServer(address)
    }

    onVisibleChanged: {
        if (visible) {
            addressField.forceActiveFocus()
            addressField.selectAll()
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: control.close()
    }

    ColumnLayout {
        id: layout
        // Not filled to the window: the window height follows this layout's implicit
        // height a polish later, and a filled layout would be spread over the old height.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12

        FishUI.TextField {
            id: addressField
            Layout.fillWidth: true
            placeholderText: qsTr("smb://server/share or sftp://user@server")
            selectByMouse: true
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
            onAccepted: control.connectTo(text)
        }

        FishUI.Label {
            visible: recentRepeater.count > 0
            Layout.topMargin: 4
            text: qsTr("Recent Servers")
            color: FishUI.Theme.disabledTextColor
            font.pixelSize: 12
        }

        Column {
            Layout.fillWidth: true
            visible: recentRepeater.count > 0

            Repeater {
                id: recentRepeater
                model: control.placesModel ? control.placesModel.recentServers : []

                delegate: Item {
                    id: recentRow
                    required property string modelData

                    width: parent.width
                    height: 32

                    HoverHandler {
                        id: rowHover
                    }

                    TapHandler {
                        onTapped: addressField.text = recentRow.modelData
                        onDoubleTapped: control.connectTo(recentRow.modelData)
                    }

                    FishUI.RoundedRectangle {
                        anchors.fill: parent
                        radius: FishUI.Theme.smallRadius
                        continuousCorners: false
                        color: addressField.text === recentRow.modelData
                               ? Qt.rgba(FishUI.Theme.textColor.r, FishUI.Theme.textColor.g,
                                         FishUI.Theme.textColor.b, 0.08)
                               : rowHover.hovered ? Qt.rgba(FishUI.Theme.textColor.r,
                                                            FishUI.Theme.textColor.g,
                                                            FishUI.Theme.textColor.b, 0.04)
                                                  : "transparent"
                    }

                    FishUI.IconItem {
                        id: serverIcon
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        themeSize: 16
                        source: "network-server"
                        color: FishUI.Theme.textColor
                    }

                    FishUI.Label {
                        anchors.left: serverIcon.right
                        anchors.leftMargin: 8
                        anchors.right: removeButton.left
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: recentRow.modelData
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                    }

                    ToolButton {
                        id: removeButton
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24
                        height: 24
                        visible: rowHover.hovered
                        Accessible.name: qsTr("Remove from Recent Servers")
                        background: Item {}

                        contentItem: Item {
                            FishUI.IconItem {
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                themeSize: 16
                                source: "edit-clear"
                                color: FishUI.Theme.textColor
                                opacity: removeButton.hovered ? 0.85 : 0.45
                            }
                        }

                        onClicked: control.placesModel.removeRecentServer(recentRow.modelData)
                    }
                }
            }
        }

        RowLayout {
            Layout.topMargin: 4
            Layout.alignment: Qt.AlignRight
            spacing: 8

            FishUI.ConfirmDialogButton {
                text: qsTr("Cancel")
                onClicked: control.close()
            }

            FishUI.ConfirmDialogButton {
                text: qsTr("Connect")
                primary: true
                enabled: addressField.text.trim().length > 0
                onClicked: control.connectTo(addressField.text)
            }
        }
    }
}
