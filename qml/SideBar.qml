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

import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

Item {
    id: sideBar

    implicitWidth: 190

    property string title
    // The header aligns its own contents to this label's text line.
    property alias titleItem: titleLabel
    property alias currentIndex: listView.currentIndex
    property alias count: listView.count
    property alias model: listView.model

    signal clicked(string path)
    signal openInNewWindow(string path)

    Fm {
        id: _fm
    }

    PlacesModel {
        id: placesModel
        onDeviceSetupDone: sideBar.clicked(filePath)    // 设备挂载上后，模拟点击了该设备以打开该页面
    }

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.backgroundColor, 1.5)
                                     : Qt.darker(FishUI.Theme.backgroundColor, 1.05)
        opacity: 0.7

        Behavior on color {
            ColorAnimation {
                duration: 250
                easing.type: Easing.Linear
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            id: titleLabel
            text: sideBar.title
            color: root.active ? FishUI.Theme.textColor : FishUI.Theme.disabledTextColor
            Layout.preferredHeight: root.header.height
            leftPadding: FishUI.Units.largeSpacing + FishUI.Units.smallSpacing
            rightPadding: FishUI.Units.largeSpacing + FishUI.Units.smallSpacing
            topPadding: FishUI.Units.smallSpacing
            bottomPadding: 0
            font.pointSize: 12
        }

        ListView {
            id: listView
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.topMargin: FishUI.Units.smallSpacing
            clip: true
            model: placesModel

            // Rounding to whole *logical* pixels is not enough at a fractional scale:
            // at 150% a 37px row pitch is 55.5 device px, so every other row lands on
            // a half pixel and its icon is resampled.
            readonly property real dpr: FishUI.Dpi.ratio
            function snap(v) {
                return Math.round(v * listView.dpr) / listView.dpr
            }

            FishUI.WheelHandler {
                target: listView
            }

            // Not leftMargin/rightMargin: Qt rounds the content item's position to a
            // whole logical pixel, so a snapped fractional margin never takes effect.
            // The inset is applied inside the rows instead.
            readonly property real sideInset: listView.snap(FishUI.Units.smallSpacing * 1.5)

            bottomMargin: listView.snap(FishUI.Units.smallSpacing)
            spacing: 2

            ScrollBar.vertical: ScrollBar {
                bottomPadding: FishUI.Units.smallSpacing
            }

            section.property: "category"
            section.delegate: Item {
                width: ListView.view.width
                height: listView.snap(FishUI.Units.fontMetrics.height + FishUI.Units.largeSpacing + FishUI.Units.smallSpacing)

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: Qt.application.layoutDirection === Qt.RightToLeft
                                        ? 0 : listView.snap(listView.sideInset + FishUI.Units.smallSpacing)
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
                width: ListView.view.width
                height: listView.snap(FishUI.Units.fontMetrics.height + FishUI.Units.largeSpacing * 1.5)

                property bool checked: sideBar.currentIndex === index
                property color hoveredColor: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.backgroundColor, 1.1)
                                                                 : Qt.darker(FishUI.Theme.backgroundColor, 1.1)
                MouseArea {
                    id: _mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
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

                    FishUI.MenuItem {
                        text: qsTr("Open")

                        onTriggered: {
                            if (model.isDevice && model.setupNeeded)
                                placesModel.requestSetup(index)
                            else
                                sideBar.clicked(model.path ? model.path : model.url)
                        }
                    }

                    FishUI.MenuItem {
                        text: qsTr("Open in new window")

                        onTriggered: {
                            sideBar.openInNewWindow(model.path ? model.path : model.url)
                        }
                    }

                    FishUI.MenuSeparator {
                        Layout.fillWidth: true
                        visible: _ejectMenuItem.visible || _umountMenuItem.visible
                    }

                    FishUI.MenuItem {
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

                    FishUI.MenuItem {
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
                    x: listView.sideInset
                    width: _item.width - listView.sideInset * 2
                    height: parent.height
                    radius: FishUI.Theme.mediumRadius
                    color: _mouseArea.pressed ? Qt.rgba(FishUI.Theme.textColor.r,
                                                       FishUI.Theme.textColor.g,
                                                       FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1) :
                           _mouseArea.containsMouse && !checked ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                          FishUI.Theme.textColor.g,
                                                                          FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.1 : 0.05) :
                           checked ? Qt.rgba(FishUI.Theme.textColor.r,
                                             FishUI.Theme.textColor.g,
                                             FishUI.Theme.textColor.b, 0.05) : "transparent"

                    smooth: true
                }

                // Positioned by hand rather than with a RowLayout: AlignVCenter computes
                // (height - 22) / 2, which lands on a half device pixel even when the
                // row height itself is snapped.
                FishUI.IconItem {
                    id: _icon
                    x: listView.snap(listView.sideInset + FishUI.Units.smallSpacing)
                    y: listView.snap((_item.height - height) / 2)
                    width: listView.snap(22)
                    height: width

                    source: model.iconName
                    color: checked ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                }

                Label {
                    id: _label
                    x: listView.snap(_icon.x + _icon.width + FishUI.Units.smallSpacing)
                    y: listView.snap((_item.height - height) / 2)
                    width: _item.width - x - listView.snap(FishUI.Units.smallSpacing)
                    text: model.name
                    color: checked ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                    elide: Text.ElideRight
                }
            }
        }
    }

    function updateSelection(path) {
        listView.currentIndex = -1

        for (var i = 0; i < listView.count; ++i) {
            if (path === listView.model.get(i).path ||
                    path === listView.model.get(i).url) {
                listView.currentIndex = i
                break
            }
        }
    }
}
