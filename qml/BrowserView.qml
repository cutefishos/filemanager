import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.0

import MeuiKit 1.0 as Meui
import Cutefish.FileManager 1.0 as FM

Item {
    id: control

    property alias model: dirModel
    property alias url: dirModel.url
    property alias currentView: viewLoader.item

    signal openPathBar

    FM.FolderModel {
        id: dirModel
        sortDirsFirst: true
        parseDesktopFiles: true
        url: dirModel.homePath()
        previews: true
        previewPlugins: []
    }

    FM.Positioner {
        id: positioner
        folderModel: dirModel
        enabled: true
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 0
        anchors.leftMargin: Meui.Theme.smallRadius / 2
        anchors.rightMargin: Meui.Theme.smallRadius
        anchors.bottomMargin: Meui.Theme.smallRadius
        radius: Meui.Theme.smallRadius
        color: Meui.Theme.backgroundColor

        Label {
            anchors.centerIn: parent
            text: qsTr("No Files")
            font.pointSize: 20
            visible: dirModel.status === FM.FolderModel.Ready && currentView.count === 0
        }
    }

    Loader {
        id: viewLoader
        anchors.fill: parent
        anchors.bottomMargin: Meui.Units.largeSpacing
        sourceComponent: switch (settings.viewMethod) {
                            case 0: return listViewBrowser
                            case 1: return gridViewBrowser
                         }
    }

    Component {
        id: listViewBrowser

        FolderListView {
            id: _listViewBrowser
            anchors.fill: parent
            model: dirModel

            leftMargin: Meui.Units.largeSpacing + Meui.Units.smallSpacing
            rightMargin: Meui.Units.largeSpacing + Meui.Units.smallSpacing
            topMargin: Meui.Units.smallSpacing
            bottomMargin: Meui.Units.largeSpacing * 2

            delegate: FolderListDelegate {
                id: listDelegate
                width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                height: 48
            }
        }
    }

    Component {
        id: gridViewBrowser

        FolderIconView {
            id: _gridViewBrowser
            anchors.fill: parent
            model: positioner

            leftMargin: Meui.Units.largeSpacing
            rightMargin: Meui.Units.largeSpacing

            delegate: FolderIconDelegate {
                id: iconDelegate
                height: _gridViewBrowser.cellHeight
                width: _gridViewBrowser.cellWidth
            }
        }
    }

    Component.onCompleted: {
        control.currentView.forceActiveFocus()
    }

    function openFolder(url) {
        dirModel.url = url
    }
}
