import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

import Cutefish.FileManager 1.0

import MeuiKit 1.0 as Meui

Meui.Window {
    id: root
    width: settings.width
    height: settings.height
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: qsTr("File Manager")

    hideHeaderOnMaximize: false
    headerBarHeight: 35 + Meui.Units.largeSpacing
    backgroundColor: Meui.Theme.secondBackgroundColor

    property QtObject settings: GlobalSettings { }

    onClosing: {
        settings.width = root.width
        settings.height = root.height
    }

    headerBar: Item {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Meui.Units.largeSpacing
            anchors.rightMargin: Meui.Units.smallSpacing
            anchors.topMargin: Meui.Units.largeSpacing
            spacing: Meui.Units.smallSpacing

            IconButton {
                Layout.fillHeight: true
                implicitWidth: height
                source: Meui.Theme.darkMode ? "qrc:/images/dark/go-previous.svg" : "qrc:/images/light/go-previous.svg"
                onClicked: _browserView.goBack()
            }

            IconButton {
                Layout.fillHeight: true
                implicitWidth: height
                source: Meui.Theme.darkMode ? "qrc:/images/dark/go-next.svg" : "qrc:/images/light/go-next.svg"
                onClicked: _browserView.goForward()
            }

            PathBar {
                id: pathBar
                Layout.fillWidth: true
                Layout.fillHeight: true
                url: _browserView.url
                onPlaceClicked: _browserView.openFolder(path)
                onPathChanged: _browserView.openFolder(path)
            }

            IconButton {
                Layout.fillHeight: true
                implicitWidth: height
                source: Meui.Theme.darkMode ? "qrc:/images/dark/grid.svg" : "qrc:/images/light/grid.svg"
                onClicked: settings.viewMethod = 1
            }

            IconButton {
                Layout.fillHeight: true
                implicitWidth: height
                source: Meui.Theme.darkMode ? "qrc:/images/dark/list.svg" : "qrc:/images/light/list.svg"
                onClicked: settings.viewMethod = 0
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Meui.Units.largeSpacing

        Item {
            id: bottomControls
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: Meui.Units.largeSpacing
                spacing: 0

                SideBar {
                    Layout.fillHeight: true
                    currentUrl: _browserView.model.url
                    onPlaceClicked: _browserView.model.url = path
                }

                BrowserView {
                    id: _browserView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // onOpenPathBar: pathBar.openEditor()
                }
            }
        }
    }
}
