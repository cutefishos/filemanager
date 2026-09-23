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
import Cutefish.DragDrop 1.0 as DragDrop

import "./Controls"

FishUI.Window {
    id: root
    width: settings.width
    height: settings.height
    minimumWidth: 600
    minimumHeight: 500
    visible: true
    title: qsTr("Files")

    background.opacity: 1
    header.height: 40
    contentTopMargin: 0

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    // The folder the first tab opens, and items in it to select once listed.
    required property string startPath
    property var initialSelection: []

    property QtObject settings: GlobalSettings { }
    property var tabs: []
    property int currentTab: 0
    readonly property var _folderPage: tabs.length ? tabs[currentTab] : null
    property string dragHoverTarget: ""
    property int dragHoverTabIndex: -1
    readonly property int springLoadDelay: 650

    function openTab(path) {
        openTabSelecting(path, [])
    }

    function openTabSelecting(path, selection) {
        var page = folderComponent.createObject(_content, {
            initialUrl: path || "",
            initialSelection: selection
        })
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

    function setDragHoverTarget(target, tabIndex) {
        if (dragHoverTarget === target && dragHoverTabIndex === tabIndex)
            return

        springLoadTimer.stop()
        dragHoverTarget = target
        dragHoverTabIndex = tabIndex
        if (target !== "")
            springLoadTimer.start()
    }

    function updateDragHoverPosition(source, x, y) {
        var backPos = source.mapToItem(_backButton, x, y)
        if (_backButton.enabled && backPos.x >= 0 && backPos.y >= 0
                && backPos.x < _backButton.width && backPos.y < _backButton.height) {
            setDragHoverTarget("back", -1)
            return
        }

        var forwardPos = source.mapToItem(_forwardButton, x, y)
        if (_forwardButton.enabled && forwardPos.x >= 0 && forwardPos.y >= 0
                && forwardPos.x < _forwardButton.width && forwardPos.y < _forwardButton.height) {
            setDragHoverTarget("forward", -1)
            return
        }

        var tabPos = source.mapToItem(tabBar, x, y)
        var tabIndex = tabBar.visible ? tabBar.indexAtPosition(tabPos.x, tabPos.y) : -1
        if (tabIndex >= 0 && tabIndex !== currentTab) {
            setDragHoverTarget("tab", tabIndex)
            return
        }

        setDragHoverTarget("", -1)
    }

    function clearDragHoverTarget() {
        springLoadTimer.stop()
        dragHoverTarget = ""
        dragHoverTabIndex = -1
    }

    function updateCurrentViewDragPosition(source, event) {
        var view = root._folderPage ? root._folderPage.currentView : null
        if (!view)
            return false

        var position = source.mapToItem(view, event.x, event.y)
        view.updateDragScrollPosition(position.y)
        return view.updateDropTarget(event, position)
    }

    function stopCurrentViewDragScroll() {
        clearDragHoverTarget()
        if (root._folderPage && root._folderPage.currentView) {
            root._folderPage.currentView.stopDragScroll()
            root._folderPage.currentView.clearDropTarget()
        }
    }

    function dropOnCurrentView(source, event, x, y) {
        var view = root._folderPage ? root._folderPage.currentView : null
        if (!view) {
            event.ignore()
            return
        }

        var position = source.mapToItem(view, x, y)
        if (position.x < 0 || position.y < 0
                || position.x > view.width || position.y > view.height) {
            root.stopCurrentViewDragScroll()
            event.ignore()
            return
        }

        root.stopCurrentViewDragScroll()
        view.drop(view, event, position)
    }

    onCurrentTabChanged: syncNavigation()
    Component.onCompleted: openTabSelecting(startPath, initialSelection)

    Timer {
        id: springLoadTimer
        objectName: "springLoadTimer"
        interval: root.springLoadDelay
        onTriggered: {
            if (root.dragHoverTarget === "tab"
                    && root.dragHoverTabIndex >= 0
                    && root.dragHoverTabIndex < root.tabs.length) {
                root.currentTab = root.dragHoverTabIndex
            } else if (root.dragHoverTarget === "back"
                       && root._folderPage && root._folderPage.canGoBack) {
                root._folderPage.goBack()
            } else if (root.dragHoverTarget === "forward"
                       && root._folderPage && root._folderPage.canGoForward) {
                root._folderPage.goForward()
            }
        }
    }

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
                text: qsTr("New Window")
                onTriggered: root._folderPage.model.openInNewWindow(root._folderPage.currentUrl)
            }

            Platform.MenuItem {
                text: qsTr("New Folder")
                onTriggered: root._folderPage.model.newFolder()
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("Properties")
                onTriggered: root._folderPage.model.openPropertiesDialog()
            }
        }

        Platform.Menu {
            title: qsTr("Edit")

            Platform.MenuItem {
                text: qsTr("Undo")
                enabled: root._folderPage && root._folderPage.model.action("undo").enabled
                onTriggered: root._folderPage.model.undo()
            }

            Platform.MenuItem {
                text: qsTr("Redo")
                enabled: root._folderPage && root._folderPage.model.action("redo").enabled
                onTriggered: root._folderPage.model.redo()
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

            Platform.MenuItem {
                text: qsTr("Select All")
                onTriggered: root._folderPage.model.selectAll()
            }
        }
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
            spacing: FishUI.Units.smallSpacing / 2

            readonly property int buttonSize: 31

            IconButton {
                id: _backButton
                objectName: "backButton"
                Layout.alignment: root.windowButtonsAlignment
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize
                enabled: _folderPage && _folderPage.canGoBack
                highlighted: root.dragHoverTarget === "back"
                iconName: "chevron-left"
                iconSize: 16
                onClicked: _folderPage.goBack()
            }

            IconButton {
                id: _forwardButton
                objectName: "forwardButton"
                Layout.alignment: root.windowButtonsAlignment
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize
                enabled: _folderPage && _folderPage.canGoForward
                highlighted: root.dragHoverTarget === "forward"
                iconName: "chevron-right"
                iconSize: 16
                onClicked: _folderPage.goForward()
            }

            FishUI.Label {
                id: _folderName
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: FishUI.Units.smallSpacing / 2
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

                iconName: settings.viewMethod === 0 ? "list" : "grid"
                iconSize: 16

                onClicked: {
                    optionsMenu.popup()
                }
            }
        }

    }

    // Cover the actual window header layer, not only its title-content slot.
    DragDrop.DropArea {
        id: _headerDragArea
        parent: root.header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.header.height
        z: 1000

        onDragEnter: function(event) {
            root.updateDragHoverPosition(_headerDragArea, event.x, event.y)
            root.updateCurrentViewDragPosition(_headerDragArea, event)
            event.accept(Qt.MoveAction)
        }
        onDragMove: function(event) {
            root.updateDragHoverPosition(_headerDragArea, event.x, event.y)
            root.updateCurrentViewDragPosition(_headerDragArea, event)
            event.accept(Qt.MoveAction)
        }
        onDragLeave: root.stopCurrentViewDragScroll()
        onDrop: {
            root.stopCurrentViewDragScroll()
            event.ignore()
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

            // The folder view runs up under the header and the tab bar, so their
            // empty parts must not reach it. Wheel events still pass through.
            MouseArea {
                z: 1
                width: parent.width
                height: root.header.height + tabBar.height
                acceptedButtons: Qt.AllButtons
                hoverEnabled: true
            }

            FolderTabBar {
                id: tabBar
                // The folder view's frosted backdrop shows through.
                color: "transparent"
                objectName: "folderTabBar"
                y: root.header.height
                width: parent.width
                z: 2
                visible: root.tabs.length > 1
                height: visible ? 38 : 0
                tabs: root.tabs
                currentIndex: root.currentTab
                springLoadIndex: root.dragHoverTarget === "tab" ? root.dragHoverTabIndex : -1
                onSelected: function(index) { root.currentTab = index }
                onCloseRequested: function(index) { root.closeTab(index) }
                onNewRequested: root.openTab(_folderPage.currentUrl)
                onDuplicateRequested: function(path) { root.openTab(path) }
                onMoveRequested: function(from, to) { root.moveTab(from, to) }
            }

            DragDrop.DropArea {
                id: _contentDragArea
                anchors.fill: parent
                z: 10

                onDragEnter: function(event) {
                    root.updateDragHoverPosition(_contentDragArea, event.x, event.y)
                    root.updateCurrentViewDragPosition(_contentDragArea, event)
                    event.accept(Qt.MoveAction)
                }
                onDragMove: function(event) {
                    root.updateDragHoverPosition(_contentDragArea, event.x, event.y)
                    root.updateCurrentViewDragPosition(_contentDragArea, event)
                    event.accept(Qt.MoveAction)
                }
                onDragLeave: root.stopCurrentViewDragScroll()
                onDrop: function(event) {
                    root.dropOnCurrentView(_contentDragArea, event, event.x, event.y)
                }
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
            bottomNavigationHeight: _navigationBar.height
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
                onItemClicked: function(path) { _folderPage.openUrl(path) }
                onEditorAccepted: function(path) { _folderPage.openUrl(path) }
                onOpenInNewTab: function(path) { root.openTab(path) }
                onOpenInNewWindow: function(path) { _folderPage.model.openInNewWindow(path) }
            }
        }

        // Include the bottom bar in drag scrolling without accepting file drops.
        DragDrop.DropArea {
            id: _navigationDragArea
            anchors.fill: parent
            z: 10

            onDragEnter: function(event) {
                root.updateDragHoverPosition(_navigationDragArea, event.x, event.y)
                root.updateCurrentViewDragPosition(_navigationDragArea, event)
                event.accept(Qt.MoveAction)
            }
            onDragMove: function(event) {
                root.updateDragHoverPosition(_navigationDragArea, event.x, event.y)
                root.updateCurrentViewDragPosition(_navigationDragArea, event)
                event.accept(Qt.MoveAction)
            }
            onDragLeave: root.stopCurrentViewDragScroll()
            onDrop: {
                root.stopCurrentViewDragScroll()
                event.ignore()
            }
        }
    }
}
