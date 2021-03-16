import QtQuick 2.12
import QtQuick.Controls 2.12
import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui

Menu {
    id: control

    property FMList currentList

    signal emptyTrashClicked()
    signal propertiesClicked()
    signal selectAllClicked()

    MenuItem {
        id: newFolderItem
        text: qsTr("New Folder")
        enabled: currentList.pathType !== FMList.TRASH_PATH
    }

    MenuSeparator {
        visible: newFolderItem.visible && pasteItem.visible
    }

    MenuItem {
        id: pasteItem
        text: qsTr("Paste")
        onTriggered: paste()
        enabled: currentList.pathType !== FMList.TRASH_PATH
    }

    MenuItem {
        text: qsTr("Select All")
        onTriggered: control.selectAllClicked()
    }

    MenuItem {
        id: terminal
        text: qsTr("Open in Terminal")
    }

    MenuItem {
        id: properties
        text: qsTr("Properties")
        onTriggered: {
            propertiesClicked()
            close()
        }
    }

    MenuItem {
        id: emptyItem
        text: qsTr("Empty Trash")
        visible: currentList.pathType === FMList.TRASH_PATH
        onTriggered: control.emptyTrashClicked()
    }

    function show(parent = control, x, y) {
        popup(parent, x, y)
    }
}
