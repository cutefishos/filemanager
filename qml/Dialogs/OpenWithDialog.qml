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

    width: 320 + FishUI.Units.largeSpacing * 2
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
        anchors.margins: FishUI.Units.largeSpacing
        spacing: FishUI.Units.largeSpacing * 2

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            model: ListModel {}
            spacing: FishUI.Units.smallSpacing * 1.5
            ScrollBar.vertical: ScrollBar {}
            clip: true

            Label {
                anchors.centerIn: parent
                text: qsTr("No applications")
                visible: listView.count === 0
            }

            delegate: Item {
                id: item
                width: ListView.view.width
                height: 30 + FishUI.Units.largeSpacing
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
                    radius: FishUI.Theme.bigRadius
                    color: isSelected ? FishUI.Theme.highlightColor
                                      : mouseArea.containsMouse ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                          FishUI.Theme.textColor.g,
                                                                          FishUI.Theme.textColor.b,
                                                                          0.1) : "transparent"
                    smooth: true
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: FishUI.Units.smallSpacing
                    spacing: FishUI.Units.smallSpacing

                    FishUI.IconItem {
                        id: icon
                        Layout.fillHeight: true
                        Layout.preferredWidth: height
                        source: model.icon
                        Layout.alignment: Qt.AlignLeft
                    }

                    Label {
                        text: model.name
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignLeft
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
        }

        RowLayout {
            spacing: FishUI.Units.largeSpacing

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
}
