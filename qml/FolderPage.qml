import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI

import "./Dialogs"

Item {
    id: folderPage

    property alias currentUrl: dirModel.url
    property Item currentView: _viewLoader.item
    property int statusBarHeight: 22

    signal requestPathEditor()

    onCurrentUrlChanged: {
        _viewLoader.item.reset()
        _viewLoader.item.forceActiveFocus()
    }

    Rectangle {
        id: _background
        anchors.fill: parent
        anchors.rightMargin: FishUI.Units.smallSpacing * 1.5
        anchors.bottomMargin: FishUI.Units.smallSpacing * 1.5
        radius: FishUI.Theme.smallRadius
        color: FishUI.Theme.backgroundColor
    }

    Label {
        id: _fileTips
        text: qsTr("Empty folder")
        font.pointSize: 15
        anchors.centerIn: parent
        visible: false
    }

    FolderModel {
        id: dirModel
        viewAdapter: viewAdapter

        Component.onCompleted: {
            if (arg)
                dirModel.url = arg
            else
                dirModel.url = dirModel.homePath()
        }
    }

    ItemViewAdapter {
        id: viewAdapter
        adapterView: _viewLoader.item
        adapterModel: _viewLoader.item.positioner ? _viewLoader.item.positioner : dirModel
        adapterIconSize: 40
        adapterVisibleArea: Qt.rect(_viewLoader.item.contentX, _viewLoader.item.contentY,
                                    _viewLoader.item.contentWidth, _viewLoader.item.contentHeight)
    }

    FishUI.DesktopMenu {
        id: folderMenu

        MenuItem {
            text: qsTr("Open")
            onTriggered: dirModel.openSelected()
        }

        MenuItem {
            text: qsTr("Properties")
            onTriggered: dirModel.openPropertiesDialog()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.bottomMargin: FishUI.Theme.smallRadius
        spacing: 0

        Loader {
            id: _viewLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: switch (settings.viewMethod) {
                             case 0: return _listViewComponent
                             case 1: return _gridViewComponent
                             }

            onSourceComponentChanged: {
                // 焦点
                _viewLoader.item.forceActiveFocus()
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
        dirModel.requestRename.connect(rename)
    }

    Component {
        id: _statusBar

        Item {
            height: statusBarHeight
            z: 999

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: FishUI.Units.smallSpacing
                anchors.rightMargin: FishUI.Units.largeSpacing
                anchors.bottomMargin: 1
                spacing: FishUI.Units.largeSpacing

                Label {
                    Layout.alignment: Qt.AlignLeft
                    font.pointSize: 10
                    text: dirModel.count === 1 ? qsTr("%1 item").arg(dirModel.count)
                                               : qsTr("%1 items").arg(dirModel.count)
                }

                Label {
                    Layout.alignment: Qt.AlignLeft
                    font.pointSize: 10
                    text: qsTr("%1 selected").arg(dirModel.selectionCound)
                    visible: dirModel.selectionCound >= 1
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Empty Trash")
                    font.pointSize: 10
                    onClicked: dirModel.emptyTrash()
                    visible: dirModel.url === "trash:/"
                    focusPolicy: Qt.NoFocus
                }
            }
        }
    }

    Component {
        id: _gridViewComponent

        FolderGridView {
            id: _gridView
            model: dirModel
            delegate: FolderGridItem {}

            leftMargin: FishUI.Units.smallSpacing
            rightMargin: FishUI.Units.largeSpacing
            topMargin: 0
            bottomMargin: FishUI.Units.smallSpacing

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
            model: dirModel

            topMargin: FishUI.Units.smallSpacing
            leftMargin: FishUI.Units.largeSpacing
            rightMargin: FishUI.Units.largeSpacing + FishUI.Theme.smallRadius
            spacing: FishUI.Units.largeSpacing

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
            color: FishUI.Theme.highlightColor

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
                dirModel.openSelected()
            else if (event.key === Qt.Key_C && event.modifiers & Qt.ControlModifier)
                dirModel.copy()
            else if (event.key === Qt.Key_X && event.modifiers & Qt.ControlModifier)
                dirModel.cut()
            else if (event.key === Qt.Key_V && event.modifiers & Qt.ControlModifier)
                dirModel.paste()
            else if (event.key === Qt.Key_F2)
                dirModel.requestRename()
            else if (event.key === Qt.Key_L && event.modifiers & Qt.ControlModifier)
                folderPage.requestPathEditor()
            else if (event.key === Qt.Key_A && event.modifiers & Qt.ControlModifier)
                dirModel.selectAll()
            else if (event.key === Qt.Key_Backspace)
                dirModel.up()
            else if (event.key === Qt.Key_Delete)
                dirModel.keyDeletePress()
        }
    }

    function openUrl(url) {
        dirModel.url = url
        _viewLoader.item.forceActiveFocus()
    }

    function goBack() {
        dirModel.goBack()
    }

    function goForward() {
        dirModel.goForward()
    }
}
