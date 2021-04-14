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

    headerBarHeight: 35 + FishUI.Units.smallSpacing * 3
    backgroundColor: FishUI.Theme.secondBackgroundColor

    property QtObject settings: GlobalSettings { }

    onClosing: {
        if (root.visibility !== Window.Maximized &&
                root.visibility !== Window.FullScreen) {
            settings.width = root.width
            settings.height = root.height
        }
    }

    headerBar: Item {
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
                    settings.viewMethod ^= 1    // reverse settings.viewMethod
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
