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
import QtQuick.Window 2.12
import QtGraphicalEffects 1.0

import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

ListView {
    id: sideBar

    signal clicked(string path)
    signal openInNewWindow(string path)

    FishUI.WheelHandler {
        target: sideBar
    }

    Fm {
        id: _fm
    }

    PlacesModel {
        id: placesModel
        onDeviceSetupDone: sideBar.clicked(filePath)    // 设备挂载上后，模拟点击了该设备以打开该页面
    }

    model: placesModel
    clip: true

    leftMargin: FishUI.Units.smallSpacing * 1.5
    rightMargin: FishUI.Units.smallSpacing * 1.5
    bottomMargin: FishUI.Units.smallSpacing
    spacing: FishUI.Units.smallSpacing

    ScrollBar.vertical: ScrollBar {
        bottomPadding: FishUI.Units.smallSpacing
    }

    highlightFollowsCurrentItem: true
    highlightMoveDuration: 0
    highlightResizeDuration : 0

    highlight: Rectangle {
        radius: FishUI.Theme.mediumRadius
        color: FishUI.Theme.secondBackgroundColor
        smooth: true

        Rectangle {
            anchors.fill: parent
            radius: FishUI.Theme.mediumRadius
            color: Qt.rgba(FishUI.Theme.highlightColor.r,
                           FishUI.Theme.highlightColor.g,
                           FishUI.Theme.highlightColor.b, FishUI.Theme.darkMode ? 0.3 : 0.2)
        }
    }

    section.property: "category"
    section.delegate: Item {
        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
        height: FishUI.Units.fontMetrics.height + FishUI.Units.largeSpacing + FishUI.Units.smallSpacing

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: Qt.application.layoutDirection === Qt.RightToLeft ? 0 : FishUI.Units.smallSpacing
            anchors.rightMargin: FishUI.Units.smallSpacing
            anchors.topMargin: FishUI.Units.largeSpacing
            anchors.bottomMargin: FishUI.Units.smallSpacing
            color: FishUI.Theme.textColor
            font.pointSize: 9
            font.bold: true
            text: section
        }
    }

    delegate: Item {
        id: _item
        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
        height: FishUI.Units.fontMetrics.height + FishUI.Units.largeSpacing * 1.5

        property bool checked: sideBar.currentIndex === index
        property color hoveredColor: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.backgroundColor, 1.1)
                                                         : Qt.darker(FishUI.Theme.backgroundColor, 1.1)
        MouseArea {
            id: _mouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: {
                if (mouse.button === Qt.LeftButton) {
                    if (model.isDevice && model.setupNeeded)
                        placesModel.requestSetup(index)
                    else
                        sideBar.clicked(model.path ? model.path : model.url)
                } else if (mouse.button === Qt.RightButton) {
                    _menu.popup()
                }
            }
        }

        FishUI.DesktopMenu {
            id: _menu

            MenuItem {
                text: qsTr("Open")

                onTriggered: {
                    if (model.isDevice && model.setupNeeded)
                        placesModel.requestSetup(index)
                    else
                        sideBar.clicked(model.path ? model.path : model.url)
                }
            }

            MenuItem {
                text: qsTr("Open in new window")

                onTriggered: {
                    sideBar.openInNewWindow(model.path ? model.path : model.url)
                }
            }

            MenuSeparator {
                Layout.fillWidth: true
                visible: _ejectMenuItem.visible || _umountMenuItem.visible
            }

            MenuItem {
                id: _ejectMenuItem
                text: qsTr("Eject")
                visible: model.isDevice &&
                         !model.setupNeeded &&
                         model.isOpticalDisc &&
                         !model.url.toString() === _fm.rootPath()

                onTriggered: {
                    placesModel.requestEject(index)
                }
            }

            MenuItem {
                id: _umountMenuItem
                text: qsTr("Unmount")
                visible: model.isDevice &&
                         !model.setupNeeded &&
                         !model.isOpticalDisc &&
                         !model.url.toString() === _fm.rootPath()

                onTriggered: {
                    placesModel.requestTeardown(index)
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: FishUI.Theme.mediumRadius
            color: _mouseArea.pressed ? Qt.rgba(FishUI.Theme.textColor.r,
                                               FishUI.Theme.textColor.g,
                                               FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1) :
                   _mouseArea.containsMouse && !checked ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                  FishUI.Theme.textColor.g,
                                                                  FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.1 : 0.05) :
                                                          "transparent"

            smooth: true
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: FishUI.Units.smallSpacing
            anchors.rightMargin: FishUI.Units.smallSpacing
            spacing: FishUI.Units.smallSpacing

            Image {
                height: 22
                width: height
                sourceSize: Qt.size(22, 22)
                // source: "qrc:/images/dark/" + model.iconPath
//                source: "qrc:/images/" + (FishUI.Theme.darkMode || checked ? "dark/" : "light/") + model.iconPath
                source: "qrc:/images/" + model.iconPath
                Layout.alignment: Qt.AlignVCenter
                smooth: false
                antialiasing: true

                layer.enabled: true
                layer.effect: ColorOverlay {
                    color: checked ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                }
            }

            Label {
                id: _label
                text: model.name
                color: checked ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    function updateSelection(path) {
        sideBar.currentIndex = -1

        for (var i = 0; i < sideBar.count; ++i) {
            if (path === sideBar.model.get(i).path ||
                    path === sideBar.model.get(i).url) {
                sideBar.currentIndex = i
                break
            }
        }
    }
}
