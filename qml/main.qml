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
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import FishUI 1.0 as FishUI

import "./Controls"

FishUI.Window {
    id: root
    width: settings.width
    height: settings.height
    minimumWidth: 900
    minimumHeight: 580
    visible: true
    title: qsTr("File Manager")

    header.height: 36 + FishUI.Units.largeSpacing

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    property QtObject settings: GlobalSettings { }

    onClosing: {
        if (root.visibility !== Window.Maximized &&
                root.visibility !== Window.FullScreen) {
            settings.width = root.width
            settings.height = root.height
        }
    }

    OptionsMenu {
        id: optionsMenu
    }

    headerItem: Item {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: FishUI.Units.smallSpacing * 1.5
            anchors.rightMargin: FishUI.Units.smallSpacing * 1.5
            anchors.topMargin: FishUI.Units.smallSpacing * 1.5
            anchors.bottomMargin: FishUI.Units.smallSpacing * 1.5

            spacing: FishUI.Units.smallSpacing

            IconButton {
                Layout.fillHeight: true
                implicitWidth: height
                source: FishUI.Theme.darkMode ? "qrc:/images/dark/go-previous.svg"
                                              : "qrc:/images/light/go-previous.svg"
                onClicked: _folderPage.goBack()
            }

            IconButton {
                Layout.fillHeight: true
                implicitWidth: height
                source: FishUI.Theme.darkMode ? "qrc:/images/dark/go-next.svg"
                                              : "qrc:/images/light/go-next.svg"
                onClicked: _folderPage.goForward()
            }

            PathBar {
                id: _pathBar
                Layout.fillWidth: true
                Layout.fillHeight: true
                onItemClicked: _folderPage.openUrl(path)
                onEditorAccepted: _folderPage.openUrl(path)
            }

            IconButton {
                Layout.fillHeight: true
                implicitWidth: height

                property var gridSource: FishUI.Theme.darkMode ? "qrc:/images/dark/grid.svg" : "qrc:/images/light/grid.svg"
                property var listSource: FishUI.Theme.darkMode ? "qrc:/images/dark/list.svg" : "qrc:/images/light/list.svg"

                source: settings.viewMethod === 0 ? listSource : gridSource

                onClicked: {
                    optionsMenu.popup()
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SideBar {
            id: _sideBar
            Layout.fillHeight: true
            width: 180 + FishUI.Units.largeSpacing
            onClicked: _folderPage.openUrl(path)
        }

        FolderPage {
            id: _folderPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            onCurrentUrlChanged: {
                _sideBar.updateSelection(currentUrl)
                _pathBar.updateUrl(currentUrl)
            }
            onRequestPathEditor: {
                _pathBar.openEditor()
            }
        }
    }
}
