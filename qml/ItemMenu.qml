import QtQuick 2.12
import QtQuick.Controls 2.12
import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui

Menu {
    id: control
    implicitWidth: 200

    property var item : ({})
    property int index : -1
    property bool isDir : false
    property bool isExec : false

    signal openClicked(var item)
    signal removeClicked(var item)
    signal copyClicked(var item)
    signal cutClicked(var item)
    signal renameClicked(var item)
    signal wallpaperClicked(var item)
    signal propertiesClicked(var item)

    MenuItem {
        text: qsTr("Open")
        onTriggered: {
            openClicked(control.item)
            close()
        }
    }

    MenuItem {
        text: qsTr("Copy")
        onTriggered: {
            copyClicked(control.item)
            close()
        }
    }

    MenuItem {
        text: qsTr("Cut")
        onTriggered: {
            cutClicked(control.item)
            close()
        }
    }

    MenuItem {
        text: qsTr("Move to Trash")
        onTriggered: {
            removeClicked(control.item)
            close()
        }
    }

    MenuSeparator {}

    MenuItem {
        text: qsTr("Rename")
        onTriggered: {
            renameClicked(control.item)
            close()
        }
    }

    MenuItem {
        text: qsTr("Open in Terminal")
    }

    MenuItem {
        id: wallpaperItem
        text: qsTr("Set As Wallpaper")
        visible: false
        onTriggered: {
            wallpaperClicked(control.item)
            close()
        }
    }

    MenuItem {
        id: properties
        text: qsTr("Properties")
        onTriggered: {
            propertiesClicked(control.item)
            close()
        }
    }

    function show(index) {
        control.item = currentFMModel.get(index)

        if (item) {
            control.index = index
            control.isDir = item.isdir === true || item.isdir === "true"
            control.isExec = item.executable === true || item.executable === "true"
            wallpaperItem.visible = item.img === "true"

            popup()
        }

    }
}
