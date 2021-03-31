import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui

import "./Dialogs"

Item {
    id: folderPage

    property alias currentUrl: folderModel.url
    property Item currentView: _viewLoader.item
    property int statusBarHeight: 30

    signal requestPathEditor()

    onCurrentUrlChanged: {
        _viewLoader.item.forceActiveFocus()
    }

    Rectangle {
        id: _background
        anchors.fill: parent
        anchors.rightMargin: Meui.Theme.smallRadius
        anchors.bottomMargin: Meui.Theme.smallRadius
        radius: Meui.Theme.smallRadius
        color: Meui.Theme.backgroundColor
    }

    Label {
        id: _fileTips
        text: qsTr("No files")
        anchors.centerIn: parent
        visible: false
    }

    FolderModel {
        id: folderModel
        viewAdapter: viewAdapter

        Component.onCompleted: {
            if (arg)
                folderModel.url = arg
            else
                folderModel.url = folderModel.homePath()
        }
    }

    ItemViewAdapter {
        id: viewAdapter
        adapterView: _viewLoader.item
        adapterModel: folderModel
        adapterIconSize: 40
        adapterVisibleArea: Qt.rect(_viewLoader.item.contentX, _viewLoader.item.contentY,
                                    _viewLoader.item.contentWidth, _viewLoader.item.contentHeight)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.bottomMargin: Meui.Theme.smallRadius
        spacing: 0

        Loader {
            id: _viewLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: switch (settings.viewMethod) {
                             case 0: return _listViewComponent
                             case 1: return _gridViewComponent
                             }
        }

        Loader {
            Layout.fillWidth: true
            sourceComponent: _statusBar
        }
    }

    function rename() {
        _viewLoader.item.rename()
    }

    Component.onCompleted: {
        folderModel.requestRename.connect(rename)
    }

    Component {
        id: _statusBar

        Item {
            height: statusBarHeight
            z: 999

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Meui.Units.largeSpacing
                anchors.rightMargin: Meui.Units.largeSpacing
                anchors.bottomMargin: 1

                Label {
                    text: folderModel.statusText
                    Layout.alignment: Qt.AlignLeft
                    elide: Text.ElideMiddle
                }

                Button {
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Empty Trash")
                    onClicked: folderModel.emptyTrash()
                    visible: folderModel.url === "trash:/"
                }
            }
        }
    }

    Component {
        id: _gridViewComponent

        FolderGridView {
            id: _gridView
            model: folderModel
            delegate: FolderGridItem {}

            leftMargin: Meui.Units.largeSpacing
            rightMargin: Meui.Units.largeSpacing
            topMargin: Meui.Units.smallSpacing
            bottomMargin: Meui.Units.smallSpacing

            onIconSizeChanged: {
                // Save
                settings.gridIconSize = _gridView.iconSize
            }

            onCountChanged: {
                _fileTips.visible = count === 0
            }
        }
    }

    Component {
        id: _listViewComponent

        FolderListView {
            id: _folderListView
            model: folderModel

            leftMargin: Meui.Units.largeSpacing
            rightMargin: Meui.Units.largeSpacing + Meui.Theme.smallRadius
            spacing: Meui.Units.largeSpacing

            onCountChanged: {
                _fileTips.visible = count === 0
            }

            delegate: FolderListItem {}
        }
    }

    Component {
        id: rubberBandObject

        RubberBand {
            id: rubberBand

            width: 0
            height: 0
            z: 99999
            color: Meui.Theme.highlightColor

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

    Connections {
        target: _viewLoader.item

        function onKeyPress(event) {
            if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
                folderModel.openSelected()
            else if (event.key === Qt.Key_C && event.modifiers & Qt.ControlModifier)
                folderModel.copy()
            else if (event.key === Qt.Key_X && event.modifiers & Qt.ControlModifier)
                folderModel.cut()
            else if (event.key === Qt.Key_V && event.modifiers & Qt.ControlModifier)
                folderModel.paste()
            else if (event.key === Qt.Key_F2)
                folderModel.requestRename()
            else if (event.key === Qt.Key_L && event.modifiers & Qt.ControlModifier)
                folderPage.requestPathEditor()
            else if (event.key === Qt.Key_A && event.modifiers & Qt.ControlModifier)
                folderModel.selectAll()
            else if (event.key === Qt.Key_Backspace)
                folderModel.up()
        }
    }

    function openUrl(url) {
        folderModel.url = url
        _viewLoader.item.forceActiveFocus()
    }

    function goBack() {
        folderModel.goBack()
    }

    function goForward() {
        folderModel.goForward()
    }
}
