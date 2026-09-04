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

    background.opacity: 1
    header.height: 40
    contentTopMargin: 0

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

    ArchiveProgressDialog {
        id: archiveProgressDialog
        archiveModel: _folderPage.model
        hostWindow: root
    }

    headerItem: Item {
        RowLayout {
            id: _headerRow
            anchors.fill: parent
            anchors.leftMargin: _sideBar.width + FishUI.Units.smallSpacing * 1.5
            anchors.rightMargin: FishUI.Units.smallSpacing
            spacing: FishUI.Units.smallSpacing

            readonly property int buttonSize: 31

            FontMetrics {
                id: _nameMetrics
                font: _folderName.font
            }

            IconButton {
                Layout.alignment: root.windowButtonsAlignment
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize
                enabled: _folderPage.canGoBack
                source: FishUI.Theme.darkMode ? "qrc:/images/dark/go-previous.svg"
                                              : "qrc:/images/light/go-previous.svg"
                onClicked: _folderPage.goBack()
            }

            IconButton {
                Layout.alignment: root.windowButtonsAlignment
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize
                enabled: _folderPage.canGoForward
                source: FishUI.Theme.darkMode ? "qrc:/images/dark/go-next.svg"
                                              : "qrc:/images/light/go-next.svg"
                onClicked: _folderPage.goForward()
            }

            Label {
                id: _folderName
                Layout.fillWidth: true
                Layout.fillHeight: true
                leftPadding: root.windowButtonsTopMargin
                topPadding: root.windowButtonsTopMargin / 2
                color: root.active ? FishUI.Theme.textColor : FishUI.Theme.disabledTextColor
                font.pointSize: 11
                text: _pathBar.currentName
            }

            IconButton {
                Layout.alignment: root.windowButtonsAlignment
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize

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
            title: root.title
            onClicked: _folderPage.openUrl(path)
            onOpenInNewWindow: _folderPage.model.openInNewWindow(path)
        }

        FolderPage {
            id: _folderPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            headerHeight: root.header.height
            bottomNavigationHeight: root.header.height
            onCurrentUrlChanged: {
                _sideBar.updateSelection(currentUrl)
                _pathBar.updateUrl(currentUrl)
            }
            onRequestPathEditor: {
                _pathBar.openEditor()
            }
        }
    }

    Item {
        id: _navigationBar
        x: _folderPage.x
        width: _folderPage.width
        height: root.header.height
        anchors.bottom: parent.bottom
        z: 3

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Qt.rgba(FishUI.Theme.textColor.r,
                           FishUI.Theme.textColor.g,
                           FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.16 : 0.12)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: FishUI.Units.smallSpacing * 1.5
            anchors.rightMargin: FishUI.Units.smallSpacing * 1.5
            anchors.topMargin: FishUI.Units.smallSpacing
            anchors.bottomMargin: FishUI.Units.smallSpacing
            spacing: 0

            PathBar {
                id: _pathBar
                Layout.fillWidth: true
                Layout.fillHeight: true
                onItemClicked: _folderPage.openUrl(path)
                onEditorAccepted: _folderPage.openUrl(path)
            }
        }
    }
}
