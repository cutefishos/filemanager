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
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0
import FishUI 1.0 as FishUI

Item {
    id: control

    property string url: main.url

    width: 400 + FishUI.Units.largeSpacing * 2
    height: _mainLayout.implicitHeight + FishUI.Units.largeSpacing * 2

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.secondBackgroundColor
    }

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
        main.close()
    }

    Keys.enabled: true
    Keys.onEscapePressed: main.close()

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent
        spacing: 0

        GridView {
            id: listView
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            model: ListModel {}
            clip: true
            ScrollBar.vertical: ScrollBar {}

            leftMargin: FishUI.Units.smallSpacing
            rightMargin: FishUI.Units.smallSpacing

            cellHeight: {
                var extraHeight = calcExtraSpacing(80, listView.Layout.preferredHeight - topMargin - bottomMargin)
                return 80 + extraHeight
            }

            cellWidth: {
                var extraWidth = calcExtraSpacing(120, listView.width - leftMargin - rightMargin)
                return 120 + extraWidth
            }

            Label {
                anchors.centerIn: parent
                text: qsTr("No applications")
                visible: listView.count === 0
            }

            delegate: Item {
                id: item
                width: GridView.view.cellWidth
                height: GridView.view.cellHeight
                scale: mouseArea.pressed ? 0.95 : 1.0

                Behavior on scale {
                    NumberAnimation {
                        duration: 100
                    }
                }

                property bool isSelected: listView.currentIndex === index

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    onDoubleClicked: control.openApp()
                    onClicked: listView.currentIndex = index
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: FishUI.Units.smallSpacing
                    radius: FishUI.Theme.mediumRadius
                    color: isSelected ? FishUI.Theme.highlightColor
                                      : mouseArea.containsMouse ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                          FishUI.Theme.textColor.g,
                                                                          FishUI.Theme.textColor.b,
                                                                          0.1) : "transparent"
                    smooth: true
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: FishUI.Units.smallSpacing
                    spacing: FishUI.Units.smallSpacing

                    FishUI.IconItem {
                        id: icon
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: height
                        source: model.icon
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: model.name
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Qt.AlignHCenter
                        color: isSelected ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
                    }
                }
            }
        }

        CheckBox {
            id: defaultCheckBox
            focusPolicy: Qt.NoFocus
            text: qsTr("Set as default")
            enabled: listView.count >= 1
            padding: 0

            Layout.leftMargin: FishUI.Units.largeSpacing
            Layout.bottomMargin: FishUI.Units.largeSpacing
        }

        RowLayout {
            spacing: FishUI.Units.largeSpacing
            Layout.leftMargin: FishUI.Units.largeSpacing
            Layout.rightMargin: FishUI.Units.largeSpacing
            Layout.bottomMargin: FishUI.Units.largeSpacing

            Button {
                text: qsTr("Cancel")
                Layout.fillWidth: true
                onClicked: main.close()
            }

            Button {
                id: doneButton
                focus: true
                flat: true
                text: qsTr("Open")
                enabled: listView.count > 0
                Layout.fillWidth: true
                onClicked: control.openApp()
            }

        }
    }

    function calcExtraSpacing(cellSize, containerSize) {
        var availableColumns = Math.floor(containerSize / cellSize)
        var extraSpacing = 0
        if (availableColumns > 0) {
            var allColumnSize = availableColumns * cellSize
            var extraSpace = Math.max(containerSize - allColumnSize, 0)
            extraSpacing = extraSpace / availableColumns
        }
        return Math.floor(extraSpacing)
    }
}
