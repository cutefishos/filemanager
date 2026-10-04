// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

import Cutefish.FileManager 1.0 as FM
import FishUI 1.0 as FishUI

import "./Dialogs"

Item {
    id: folderPage

    property alias currentUrl: dirModel.url
    property alias model: dirModel
    property alias canGoBack: dirModel.canGoBack
    property alias canGoForward: dirModel.canGoForward
    property Item currentView: _viewLoader.item
    property int headerHeight: 0
    property string initialUrl: ""
    property var initialSelection: []
    // Back and forward history carried over from a tab in another window.
    property var initialHistory: null
    // Scroll offset to return to in a folder reached by back or forward,
    // applied once enough of the folder is listed.
    property real pendingViewPosition: NaN
    readonly property string tabTitle: {
        var path = currentUrl.toString()
        if (path.indexOf("trash:/") === 0)
            return qsTr("Trash")
        if (path.indexOf("file://") === 0)
            path = path.substring(7)
        path = path.replace(/\/+$/, "")
        var standard = dirModel.standardFolderName(path)
        if (standard)
            return standard
        var name = path.substring(path.lastIndexOf("/") + 1)
        try { return decodeURIComponent(name) || "/" } catch (error) { return name || "/" }
    }

    FM.PreviewClient {
        id: previewClient
        objectName: "previewClient"
    }

    signal requestPathEditor()
    signal openInNewTab(string path)
    signal closeRequested()

    onCurrentUrlChanged: {
        if (!_viewLoader.item)
            return

        _viewLoader.item.reset()
        pendingViewPosition = dirModel.viewPosition()
        focusView()
    }


    Rectangle {
        id: _background
        anchors.fill: parent
        color: FishUI.Theme.secondBackgroundColor
    }

    FishUI.Label {
        id: _fileTips
        text: qsTr("Empty folder")
        font.pointSize: 15
        anchors.centerIn: parent
        visible: dirModel.status === FM.FolderModel.Ready
                 && _viewLoader.status === Loader.Ready
                 && dirModel.count === 0
    }

    BusyIndicator {
        anchors.centerIn: parent
        width: 32
        height: 32
        running: dirModel.status === FM.FolderModel.Listing && dirModel.count === 0
        visible: running
    }

    FM.FolderModel {
        id: dirModel
        viewAdapter: viewAdapter
        sortMode: settings.sortMode
        sortDesc: settings.sortDesc
        // showHiddenFiles: settings.showHiddenFiles

        Component.onCompleted: {
            if (folderPage.initialUrl)
                dirModel.url = folderPage.initialUrl
            else
                dirModel.url = dirModel.homePath()

            if (folderPage.initialHistory)
                dirModel.restoreHistory(folderPage.initialHistory.back, folderPage.initialHistory.forward)

            if (folderPage.initialSelection.length > 0)
                dirModel.selectUrlsWhenListed(folderPage.initialSelection)
        }

        onOpenTabRequested: function(path) { folderPage.openInNewTab(path) }

        // For new folder rename.
        onCurrentIndexChanged: {
            if (_viewLoader.item)
                _viewLoader.item.currentIndex = dirModel.currentIndex
        }
    }

    Connections {
        target: dirModel

        function onNotification(text) {
            root.showPassiveNotification(text, 3000)
        }

        function onAboutToLeaveFolder() {
            // Left again before the offset was reached: it still stands.
            if (_viewLoader.item)
                dirModel.setViewPosition(isNaN(folderPage.pendingViewPosition)
                                         ? _viewLoader.item.contentY : folderPage.pendingViewPosition)
            folderPage.pendingViewPosition = NaN
        }

        function onListingCompleted() {
            folderPage.restoreViewPosition(true)
        }

        // Scroll to item.
        function onScrollToItem(index) {
            if (_viewLoader.item)
                _viewLoader.item.currentIndex = index
        }
    }

    FM.ItemViewAdapter {
        id: viewAdapter
        adapterView: _viewLoader.item
    }

    FolderContextMenu {
        id: folderMenu
        folderModel: dirModel
    }

    // The view runs up under the window header, which floats over it.
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Loader {
            id: _viewLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            asynchronous: true
            sourceComponent: switch (settings.viewMethod) {
                             case 0: return _listViewComponent
                             case 1: return _gridViewComponent
                             }

            onLoaded: {
                folderPage.focusView()
                shortCut.install(_viewLoader.item)
                previewClient.attach(dirModel, _viewLoader.item)
            }
        }
    }

    function rename() {
        _viewLoader.item.rename()
    }

    Component.onCompleted: {
        dirModel.requestRename.connect(rename)
    }

    Component {
        id: _gridViewComponent

        FolderGridView {
            id: _gridView
            model: dirModel
            delegate: FolderGridItem {}

            onCountChanged: folderPage.restoreViewPosition(false)

            onContextMenuRequested: folderMenu.popup()

            topInset: folderPage.headerHeight
            leftMargin: FishUI.Units.smallSpacing
            rightMargin: FishUI.Units.largeSpacing
            topMargin: topInset + FishUI.Units.smallSpacing
            bottomMargin: FishUI.Units.smallSpacing

            onIconSizeChanged: {
                // Save
                settings.gridIconSize = _gridView.iconSize
            }

        }
    }

    Component {
        id: _listViewComponent

        FolderListView {
            id: _folderListView
            model: dirModel

            onCountChanged: folderPage.restoreViewPosition(false)

            onContextMenuRequested: folderMenu.popup()

            topInset: folderPage.headerHeight + FishUI.Units.smallSpacing
            topMargin: _folderListView.headerHeight + 2
            leftMargin: FishUI.Units.smallSpacing
            rightMargin: FishUI.Units.largeSpacing
            bottomMargin: FishUI.Units.smallSpacing

            delegate: FolderListItem {}
        }
    }

    Component {
        id: rubberBandObject

        FM.RubberBand {
            id: rubberBand

            width: 0
            height: 0
            z: 99999
            color: FishUI.Theme.darkMode ? "white" : FishUI.Theme.disabledTextColor

            function close() {
                opacityAnimation.restart()
            }

            OpacityAnimator {
                id: opacityAnimation
                target: rubberBand
                to: 0
                from: 1
                duration: 150

                easing {
                    bezierCurve: [0.4, 0.0, 1, 1]
                    type: Easing.Bezier
                }

                onFinished: {
                    rubberBand.visible = false
                    rubberBand.enabled = false
                    rubberBand.destroy()
                }
            }
        }
    }

    FM.ShortCut {
        id: shortCut
        previewEnabled: true
        onPreview: previewClient.toggle()

        onOpen: {
            dirModel.openSelected()
        }
        onCopy: {
            dirModel.copy()
        }
        onCut: {
            dirModel.cut()
        }
        onPaste: {
            dirModel.paste()
        }
        onRename: {
            dirModel.requestRename()
        }
        onOpenPathEditor: {
            folderPage.requestPathEditor()
        }
        onSelectAll: {
            dirModel.selectAll()
        }
        onBackspace: {
            dirModel.up()
        }
        onUp: {
            dirModel.up()
        }
        onBack: {
            dirModel.goBack()
        }
        onForward: {
            dirModel.goForward()
        }
        onProperties: {
            dirModel.openPropertiesDialog()
        }
        onDeleteFile: {
            dirModel.keyDeletePress()
        }
        onRefresh: {
            dirModel.refresh()
        }
        onKeyPressed: {
            dirModel.keyboardSearch(text)
        }
        onShowHidden: {
            dirModel.showHiddenFiles = !dirModel.showHiddenFiles
        }
        onClose: {
            folderPage.closeRequested()
        }
        onUndo: {
            dirModel.undo()
        }
        onRedo: {
            dirModel.redo()
        }
    }

    function openUrl(url) {
        dirModel.url = url
        focusView()
    }

    function focusView() {
        if (visible && _viewLoader.item)
            _viewLoader.item.forceActiveFocus()
    }

    // Rows arrive in batches: scroll as soon as the saved offset is in reach
    // so the folder does not first show its top, and settle for the nearest
    // offset once the listing is complete.
    function restoreViewPosition(complete) {
        var view = _viewLoader.item
        if (isNaN(pendingViewPosition) || !view)
            return

        view.forceLayout()
        var minimum = view.originY - view.topMargin
        var maximum = Math.max(minimum, view.contentHeight + view.bottomMargin - view.height + view.originY)
        if (!complete && pendingViewPosition > maximum)
            return

        view.contentY = Math.max(minimum, Math.min(maximum, pendingViewPosition))
        pendingViewPosition = NaN
    }

    function goBack() {
        dirModel.goBack()
    }

    function goForward() {
        dirModel.goForward()
    }
}
