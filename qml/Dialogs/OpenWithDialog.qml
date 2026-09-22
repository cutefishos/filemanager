/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
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
import QtQuick.Controls 2.4
import QtQuick.Window 2.12
import QtQuick.Layouts 1.3
import FishUI 1.0 as FishUI

FishUI.Window {
    id: control

    property string url: main.url
    property int contentWidth: 400 + FishUI.Units.largeSpacing * 2
    property int contentHeight: _mainLayout.implicitHeight + 32

    title: "Open With"
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.WindowModal
    windowButtonsTopMargin: 8
    windowButtonsRightMargin: 8
    header.height: 44
    contentTopMargin: 0
    minimizeButtonVisible: false
    visible: false

    width: contentWidth
    height: contentHeight
    minimumWidth: contentWidth
    minimumHeight: contentHeight
    maximumWidth: contentWidth
    maximumHeight: contentHeight

    background.color: FishUI.Theme.secondBackgroundColor

    Component.onCompleted: {
        var items = mimeAppManager.recommendedApps(control.url)

        for (var i in items) {
            listView.model.append(items[i])
        }

        defaultCheckBox.checked = false
        doneButton.focus = true
    }

    function openApp() {
        if (defaultCheckBox.checked)
            mimeAppManager.setDefaultAppForFile(control.url, listView.model.get(listView.currentIndex).desktopFile)

        launcher.launchApp(listView.model.get(listView.currentIndex).desktopFile, control.url)
        control.close()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: control.close()
    }

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        FishUI.Label {
            Layout.fillWidth: true
            Layout.rightMargin: 32
            text: control.title
            textFormat: Text.PlainText
            font: Qt.font({ family: FishUI.Theme.fontFamily, pixelSize: 14, weight: Font.DemiBold })
            color: FishUI.Theme.textColor
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            model: ListModel {}
            clip: true
            FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

            spacing: 4
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationEnabled: true

            FishUI.Label {
                anchors.centerIn: parent
                text: qsTr("No applications")
                visible: listView.count === 0
            }

            delegate: Item {
                id: item
                width: ListView.view.width
                height: 52

                property bool isSelected: listView.currentIndex === index

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    onDoubleClicked: {
                        listView.currentIndex = index
                        control.openApp()
                    }
                    onClicked: listView.currentIndex = index
                }

                Rectangle {
                    anchors.fill: parent
                    radius: FishUI.Theme.mediumRadius
                    color: isSelected ? FishUI.Theme.highlightColor
                                      : mouseArea.containsMouse ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                          FishUI.Theme.textColor.g,
                                                                          FishUI.Theme.textColor.b,
                                                                          0.1) : "transparent"
                    smooth: true
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 12

                    FishUI.IconItem {
                        id: icon
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: 32
                        source: model.icon
                    }

                    FishUI.Label {
                        text: model.name
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        verticalAlignment: Text.AlignVCenter
                        textFormat: Text.PlainText
                        color: isSelected ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
                    }
                }
            }
        }

        FishUI.CheckBox {
            id: defaultCheckBox
            focusPolicy: Qt.NoFocus
            text: qsTr("Set as default")
            enabled: listView.count >= 1
            padding: 0
        }

        RowLayout {
            spacing: 8
            Layout.alignment: Qt.AlignRight

            FishUI.ConfirmDialogButton {
                text: qsTr("Cancel")
                onClicked: control.close()
            }

            FishUI.ConfirmDialogButton {
                id: doneButton
                focus: true
                primary: true
                text: qsTr("Open")
                enabled: listView.count > 0
                onClicked: control.openApp()
            }

        }
    }

}
