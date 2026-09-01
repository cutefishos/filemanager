import QtQuick 2.12

import FishUI 1.0 as FishUI

FishUI.DesktopMenu {
    id: control

    property var folderModel: null
    property bool hasSelection: folderModel ? folderModel.selectionCount > 0 : false

    FishUI.MenuItem {
        id: newFolderItem
        property var modelAction: control.folderModel ? control.folderModel.action("newFolder") : null
        text: modelAction ? modelAction.text : qsTr("New Folder")
        visible: !control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: newDocumentsItem
        text: qsTr("New Documents")
        visible: !control.hasSelection && newTextFileItem.visible

        childMenu: FishUI.DesktopMenu {
            id: newDocumentsMenu

            FishUI.MenuItem {
                id: newTextFileItem
                property var modelAction: control.folderModel ? control.folderModel.action("newTextFile") : null
                text: modelAction ? modelAction.text : qsTr("New Text")
                visible: modelAction && modelAction.visible
                enabled: modelAction ? modelAction.enabled : false
                onTriggered: modelAction.trigger()
            }
        }
    }

    FishUI.MenuSeparator {
        visible: !control.hasSelection &&
                 (newFolderItem.visible || newDocumentsItem.visible)
    }

    FishUI.MenuItem {
        id: pasteItem
        property var modelAction: control.folderModel ? control.folderModel.action("paste") : null
        text: modelAction ? modelAction.text : qsTr("Paste")
        visible: !control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: selectAllItem
        text: qsTr("Select All")
        visible: !control.hasSelection
        onTriggered: control.folderModel.selectAll()
    }

    FishUI.MenuSeparator {
        visible: !control.hasSelection &&
                 (terminalItem.visible || changeBackgroundItem.visible)
    }

    FishUI.MenuItem {
        id: terminalItem
        property var modelAction: control.folderModel ? control.folderModel.action("terminal") : null
        text: modelAction ? modelAction.text : qsTr("Open in Terminal")
        visible: !control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: changeBackgroundItem
        property var modelAction: control.folderModel ? control.folderModel.action("changeBackground") : null
        text: modelAction ? modelAction.text : qsTr("Change background")
        visible: !control.hasSelection && control.folderModel && control.folderModel.isDesktop &&
                 modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuSeparator {
        visible: !control.hasSelection && showHiddenItem.visible
    }

    FishUI.MenuItem {
        id: showHiddenItem
        property var modelAction: control.folderModel ? control.folderModel.action("showHidden") : null
        text: modelAction ? modelAction.text : qsTr("Show hidden files")
        visible: !control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        checkable: true
        checked: modelAction ? modelAction.checked : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuSeparator {
        visible: !control.hasSelection && emptyTrashItem.visible
    }

    FishUI.MenuItem {
        id: emptyTrashItem
        property var modelAction: control.folderModel ? control.folderModel.action("emptyTrash") : null
        text: modelAction ? modelAction.text : qsTr("Empty Trash")
        visible: !control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: restoreItem
        property var modelAction: control.folderModel ? control.folderModel.action("restore") : null
        text: modelAction ? modelAction.text : qsTr("Restore")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: openItem
        property var modelAction: control.folderModel ? control.folderModel.action("open") : null
        text: modelAction ? modelAction.text : qsTr("Open")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: openInNewWindowItem
        property var modelAction: control.folderModel ? control.folderModel.action("openInNewWindow") : null
        text: modelAction ? modelAction.text : qsTr("Open in new window")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: openWithItem
        property var modelAction: control.folderModel ? control.folderModel.action("openWith") : null
        text: modelAction ? modelAction.text : qsTr("Open with")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: compressItem
        property var modelAction: control.folderModel ? control.folderModel.action("compress") : null
        text: modelAction ? modelAction.text : qsTr("Compress")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuSeparator {
        visible: control.hasSelection &&
                 (cutItem.visible || copyItem.visible || trashItem.visible ||
                  deleteItem.visible || renameItem.visible)
    }

    FishUI.MenuItem {
        id: cutItem
        property var modelAction: control.folderModel ? control.folderModel.action("cut") : null
        text: modelAction ? modelAction.text : qsTr("Cut")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: copyItem
        property var modelAction: control.folderModel ? control.folderModel.action("copy") : null
        text: modelAction ? modelAction.text : qsTr("Copy")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: trashItem
        property var modelAction: control.folderModel ? control.folderModel.action("trash") : null
        text: modelAction ? modelAction.text : qsTr("Move To Trash")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: deleteItem
        property var modelAction: control.folderModel ? control.folderModel.action("del") : null
        text: modelAction ? modelAction.text : qsTr("Delete")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: renameItem
        property var modelAction: control.folderModel ? control.folderModel.action("rename") : null
        text: modelAction ? modelAction.text : qsTr("Rename")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuSeparator {
        visible: control.hasSelection &&
                 (terminalSelectedItem.visible || wallpaperItem.visible)
    }

    FishUI.MenuItem {
        id: terminalSelectedItem
        property var modelAction: control.folderModel ? control.folderModel.action("terminal") : null
        text: modelAction ? modelAction.text : qsTr("Open in Terminal")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuItem {
        id: wallpaperItem
        property var modelAction: control.folderModel ? control.folderModel.action("wallpaper") : null
        text: modelAction ? modelAction.text : qsTr("Set as Wallpaper")
        visible: control.hasSelection && modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    FishUI.MenuSeparator {
        visible: propertiesItem.visible
    }

    FishUI.MenuItem {
        id: propertiesItem
        property var modelAction: control.folderModel ? control.folderModel.action("properties") : null
        text: modelAction ? modelAction.text : qsTr("Properties")
        visible: modelAction && modelAction.visible
        enabled: modelAction ? modelAction.enabled : false
        onTriggered: modelAction.trigger()
    }

    onVisibleChanged: {
        if (visible && folderModel)
            folderModel.prepareContextMenu()
    }
}
