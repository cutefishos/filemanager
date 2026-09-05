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
import Qt.labs.platform 1.0 as Platform
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
    property var tabs: []
    property int currentTab: 0
    readonly property var _folderPage: tabs.length ? tabs[currentTab] : null

    function openTab(path) {
        var page = folderComponent.createObject(_content, { initialUrl: path || "" })
        if (!page)
            return
        tabs = tabs.concat([page])
        currentTab = tabs.length - 1
        syncNavigation()
    }

    function closeTab(index) {
        if (tabs.length === 1) {
            root.close()
            return
        }
        var page = tabs[index]
        var remaining = tabs.slice()
        remaining.splice(index, 1)
        // The list has to shrink before the index moves, or the sync that
        // follows the index change still sees the tab being closed.
        tabs = remaining
        currentTab = Math.max(0, currentTab - (index <= currentTab ? 1 : 0))
        page.destroy()
        syncNavigation()
    }

    function syncNavigation() {
        // Not _folderPage: its binding on currentTab has not been re-evaluated
        // yet when this runs from onCurrentTabChanged, so it still holds the
        // tab we are leaving.
        var page = tabs.length ? tabs[currentTab] : null
        if (!page)
            return
        _pathBar.closeEditor()
        _sideBar.updateSelection(page.currentUrl)
        _pathBar.updateUrl(page.currentUrl)
        page.focusView()
    }

    function moveTab(from, to) {
        if (from === to || from < 0 || to < 0 || from >= tabs.length || to >= tabs.length)
            return
        var activePage = _folderPage
        var reordered = tabs.slice()
        reordered.splice(to, 0, reordered.splice(from, 1)[0])
        tabs = reordered
        currentTab = reordered.indexOf(activePage)
        syncNavigation()
    }

    onCurrentTabChanged: syncNavigation()
    Component.onCompleted: openTab(arg)

    Shortcut { sequence: "Ctrl+T"; onActivated: root.openTab(_folderPage.currentUrl) }
    Shortcut { sequence: "Ctrl+W"; onActivated: root.closeTab(root.currentTab) }
    Shortcut { sequence: "Ctrl+Tab"; onActivated: root.currentTab = (root.currentTab + 1) % root.tabs.length }
    Shortcut { sequence: "Ctrl+Shift+Tab"; onActivated: root.currentTab = (root.currentTab + root.tabs.length - 1) % root.tabs.length }

    onClosing: {
        if (root.visibility !== Window.Maximized &&
                root.visibility !== Window.FullScreen) {
            settings.width = root.width
            settings.height = root.height
        }
    }

    Platform.MenuBar {
        id: appMenu
        objectName: "applicationMenuBar"
        window: root

        Platform.Menu {
            title: qsTr("File")

            Platform.MenuItem {
                text: qsTr("New Tab")
                onTriggered: root.openTab(root._folderPage.currentUrl)
            }

            Platform.MenuItem {
                text: qsTr("Open In New Tab")
                enabled: root._folderPage && root._folderPage.model.action("openInNewTab").visible
                onTriggered: root._folderPage.model.openInNewTab()
            }

            Platform.MenuItem {
                text: qsTr("Close Tab")
                onTriggered: root.closeTab(root.currentTab)
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("New Folder")
                onTriggered: root._folderPage.model.newFolder()
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("Properties")
                onTriggered: root._folderPage.model.openPropertiesDialog()
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("Quit")
                onTriggered: root.close()
            }
        }

        Platform.Menu {
            title: qsTr("Edit")

            Platform.MenuItem {
                text: qsTr("Select All")
                onTriggered: root._folderPage.model.selectAll()
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("Cut")
                onTriggered: root._folderPage.model.cut()
            }

            Platform.MenuItem {
                text: qsTr("Copy")
                onTriggered: root._folderPage.model.copy()
            }

            Platform.MenuItem {
                text: qsTr("Paste")
                onTriggered: root._folderPage.model.paste()
            }
        }

        Platform.Menu {
            title: qsTr("Help")

            Platform.MenuItem {
                text: qsTr("About")
                onTriggered: _aboutDialog.show()
            }
        }
    }

    FishUI.AboutDialog {
        id: _aboutDialog
        name: qsTr("File Manager")
        description: qsTr("A file manager designed for CutefishOS.")
        iconSource: "image://icontheme/file-system-manager"
    }

    OptionsMenu {
        id: optionsMenu
    }

    ArchiveProgressDialog {
        id: archiveProgressDialog
        archiveModel: _folderPage ? _folderPage.model : null
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
                enabled: _folderPage && _folderPage.canGoBack
                source: FishUI.Theme.darkMode ? "qrc:/images/dark/go-previous.svg"
                                              : "qrc:/images/light/go-previous.svg"
                onClicked: _folderPage.goBack()
            }

            IconButton {
                Layout.alignment: root.windowButtonsAlignment
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize
                enabled: _folderPage && _folderPage.canGoForward
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
            objectName: "sideBar"
            Layout.fillHeight: true
            title: root.title
            onClicked: _folderPage.openUrl(path)
            onOpenInNewWindow: _folderPage.model.openInNewWindow(path)
            onOpenInNewTab: root.openTab(path)
        }

        Item {
            id: _content
            Layout.fillWidth: true
            Layout.fillHeight: true

            FolderTabBar {
                id: tabBar
                color: FishUI.Theme.secondBackgroundColor
                objectName: "folderTabBar"
                y: root.header.height
                width: parent.width
                z: 2
                visible: root.tabs.length > 1
                height: visible ? 38 : 0
                tabs: root.tabs
                currentIndex: root.currentTab
                onSelected: function(index) { root.currentTab = index }
                onCloseRequested: function(index) { root.closeTab(index) }
                onNewRequested: root.openTab(_folderPage.currentUrl)
                onDuplicateRequested: function(path) { root.openTab(path) }
                onMoveRequested: function(from, to) { root.moveTab(from, to) }
            }
        }
    }

    Component {
        id: folderComponent

        FolderPage {
            anchors.fill: parent
            visible: root._folderPage === this
            enabled: visible
            headerHeight: root.header.height + tabBar.height
            bottomNavigationHeight: root.header.height
            onOpenInNewTab: function(path) { root.openTab(path) }
            onCloseRequested: root.closeTab(root.currentTab)
            onCurrentUrlChanged: {
                if (visible)
                    root.syncNavigation()
            }
            onRequestPathEditor: {
                _pathBar.openEditor()
            }
        }
    }

    Item {
        id: _navigationBar
        x: _content.x
        width: _content.width
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
                onOpenInNewTab: root.openTab(path)
                onOpenInNewWindow: _folderPage.model.openInNewWindow(path)
            }
        }
    }
}
