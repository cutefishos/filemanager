// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import Qt.labs.platform 1.0 as Platform
import FishUI 1.0 as FishUI
import Cutefish.DragDrop 1.0 as DragDrop
import Cutefish.FileManager 1.0

import "./Controls"

FishUI.Window {
    id: root
    width: 900
    height: 580
    minimumWidth: 600
    minimumHeight: 500
    visible: true
    title: qsTr("Files")

    // Inactive windows keep the blur and halve the see-through instead of going opaque.
    background.color: Qt.rgba(FishUI.Theme.backgroundColor.r,
                              FishUI.Theme.backgroundColor.g,
                              FishUI.Theme.backgroundColor.b,
                              !blurAvailable ? 1
                              : FishUI.Theme.darkMode ? (active ? 0.85 : 0.92)
                                                      : (active ? 0.8 : 0.9))
    header.height: headerInset
    contentTopMargin: 0

    // Room for the header buttons with as much space below them as above;
    // content starts under it and its frosted backdrop covers it.
    readonly property real headerInset: windowButtonsTopMargin * 2 + _headerRow.buttonSize

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    // The folder the first tab opens, and items in it to select once listed.
    required property string startPath
    property var initialSelection: []
    // Back and forward history for the first tab, when it was pulled out of another window.
    property var initialHistory: null

    // What a tab dragged between windows carries.
    readonly property string tabMimeType: "application/x-cutefish-filemanager-tab"
    // A tab is being pulled out: the tab bar stays up, even with one tab
    // left, so the tab can be dragged back into it.
    property bool detaching: false

    property QtObject settings: GlobalSettings { }

    Fm { id: _fm }
    property var tabs: []
    property int currentTab: 0
    readonly property var _folderPage: tabs.length ? tabs[currentTab] : null
    property string dragHoverTarget: ""
    property int dragHoverTabIndex: -1
    readonly property int springLoadDelay: 650

    function openTab(path) {
        openTabSelecting(path, [])
    }

    function openTabSelecting(path, selection, history, index) {
        var page = folderComponent.createObject(_content, {
            initialUrl: path || "",
            initialSelection: selection,
            initialHistory: history || null
        })
        if (!page)
            return
        insertPage(page, index === undefined ? tabs.length : index)
    }

    function insertPage(page, index) {
        var reordered = tabs.slice()
        reordered.splice(index, 0, page)
        tabs = reordered
        currentTab = index
        syncNavigation()
    }

    // The first tab as a window pulled out of another hands it on: where it
    // is, how it got there and what is selected.
    function tabState() {
        var page = tabs[0]
        var state = page.model.tabState()
        state.url = page.currentUrl
        return state
    }

    // Takes the tab of a window pulled out of another, which goes once the
    // drag it is carried by ends.
    function adoptTab(window, index) {
        var state = window.tabState()
        openTabSelecting(state.url, state.selection, { back: state.back, forward: state.forward }, index)
        window.close()
        root.requestActivate()
    }

    // Opens the tab in a window of its own, which follows the pointer until
    // it is dropped. Escape puts the tab back. `position` is where the
    // pointer is in this window.
    function detachTab(index, position) {
        if (index < 0 || index >= tabs.length || tabs.length < 2)
            return

        // Out of the bar but alive, so Escape puts it back as it was.
        detaching = true
        var page = tabs[index]
        var state = page.model.tabState()
        var remaining = tabs.slice()
        remaining.splice(index, 1)
        // The index first: it only ever moves down, so it stays in range of
        // both lists and the current page is never undefined in between.
        currentTab = Math.max(0, currentTab - (index <= currentTab ? 1 : 0))
        tabs = remaining
        syncNavigation()

        var window = _fm.openWindow({
            startPath: page.currentUrl,
            initialSelection: state.selection,
            initialHistory: { back: state.back, forward: state.forward },
            width: root.width,
            height: root.height,
            restorationBehavior: FishUI.Window.Disabled
        })
        if (!window) {
            detaching = false
            insertPage(page, index)
            return
        }

        // The pointer stays over the new window's title bar, as far across
        // as it is over this one.
        var offset = Qt.point(Math.min(position.x, window.width - 1), window.header.height / 2)
        var action = FishUI.TabTransfer.drag(window, offset, tabMimeType)
        detaching = false

        // A window that took the tab has closed this one already.
        if (action === Qt.IgnoreAction && window.visible) {
            window.close()
            insertPage(page, index)
        } else {
            page.destroy()
            if (window.visible)
                window.requestActivate()
        }
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
        _pathBar.folderModel = page.model
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
    Component.onCompleted: openTabSelecting(startPath, initialSelection, initialHistory)

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
    Shortcut { sequence: StandardKey.Find; onActivated: _pathBar.openSearch() }

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
        trash: root._folderPage ? root._folderPage.model.trash : false
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
            anchors.rightMargin: root.windowButtonsSpacing
            spacing: root.windowButtonsSpacing

            readonly property int buttonSize: root.header.buttonSize

            IconButton {
                id: _backButton
                objectName: "backButton"
                Layout.alignment: Qt.AlignTop
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
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize
                enabled: _folderPage && _folderPage.canGoForward
                highlighted: root.dragHoverTarget === "forward"
                iconName: "chevron-right"
                iconSize: 16
                onClicked: _folderPage.goForward()
            }

            PathBar {
                id: _pathBar
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredHeight: _headerRow.buttonSize
                onItemClicked: function(path) { _folderPage.openUrl(path) }
                onEditorAccepted: function(path) { _folderPage.openUrl(path) }
                onOpenInNewTab: function(path) { root.openTab(path) }
                onOpenInNewWindow: function(path) { _folderPage.model.openInNewWindow(path) }
                onFinished: _folderPage.focusView()
            }

            IconButton {
                objectName: "searchButton"
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize
                iconName: "search"
                iconSize: 16
                onClicked: _pathBar.openSearch()
            }

            IconButton {
                id: _optionsButton
                Layout.alignment: root.windowButtonsAlignment
                Layout.topMargin: root.windowButtonsTopMargin
                Layout.preferredWidth: _headerRow.buttonSize
                Layout.preferredHeight: _headerRow.buttonSize

                artwork: "menu"
                iconSize: 24
                checked: optionsMenu.visible

                onClicked: {
                    optionsMenu.popupFor(_optionsButton.background, FishUI.Units.smallSpacing / 2)
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
        onDrop: function(event) {
            root.stopCurrentViewDragScroll()
            event.ignore()
        }
    }

    // Takes a tab dragged out of another window over the title bar or the
    // tabs. While it is there the tab bar shows it where it would drop, and
    // the compositor stops drawing the window it is carried in.
    DropArea {
        parent: root.contentItem
        width: parent.width
        height: root.headerInset + tabBar.height
        z: 10000
        keys: [root.tabMimeType]

        onEntered: function(drag) {
            drag.accepted = drag.source && drag.source !== root
                    && drag.source.tabMimeType === root.tabMimeType
            if (!drag.accepted)
                return
            tabBar.beginIncoming(drag.source.tabs[0].tabTitle, mapToItem(tabBar, drag.x, drag.y).x)
        }
        onPositionChanged: function(drag) {
            if (tabBar.incoming)
                tabBar.incomingX = mapToItem(tabBar, drag.x, drag.y).x
        }
        onExited: tabBar.endIncoming(false)
        onDropped: function(drop) {
            var index = tabBar.incoming ? tabBar.incomingIndex : root.tabs.length
            tabBar.endIncoming(true)
            root.adoptTab(drop.source, index)
            drop.accept(Qt.MoveAction)
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
            onClicked: function(path) { _folderPage.openUrl(path) }
            onOpenInNewWindow: function(path) { _folderPage.model.openInNewWindow(path) }
            onOpenInNewTab: function(path) { root.openTab(path) }
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
                height: root.headerInset + tabBar.height + scopeBar.height
                acceptedButtons: Qt.AllButtons
                hoverEnabled: true
            }

            FolderTabBar {
                id: tabBar
                // The folder view's frosted backdrop shows through.
                color: "transparent"
                objectName: "folderTabBar"
                y: root.headerInset
                width: parent.width
                z: 2
                visible: root.tabs.length > 1 || incoming || root.detaching
                height: visible ? 38 : 0
                tabs: root.tabs
                currentIndex: root.currentTab
                springLoadIndex: root.dragHoverTarget === "tab" ? root.dragHoverTabIndex : -1
                onSelected: function(index) { root.currentTab = index }
                onCloseRequested: function(index) { root.closeTab(index) }
                onNewRequested: root.openTab(_folderPage.currentUrl)
                onDuplicateRequested: function(path) { root.openTab(path) }
                onMoveRequested: function(from, to) { root.moveTab(from, to) }
                // Later, not from inside the tab's own handler: taking the tab
                // out destroys the delegate that is running it.
                onDetachRequested: function(index, position) {
                    Qt.callLater(root.detachTab, index, position)
                }
            }

            SearchScopeBar {
                id: scopeBar
                objectName: "searchScopeBar"
                y: root.headerInset + tabBar.height
                width: parent.width
                z: 2
                visible: root._folderPage && root._folderPage.model.searchText.length > 0
                height: visible ? 34 : 0
                folderModel: root._folderPage ? root._folderPage.model : null
                folderName: root._folderPage ? root._folderPage.folderName : ""
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
            headerHeight: root.headerInset + tabBar.height + scopeBar.height
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
}
