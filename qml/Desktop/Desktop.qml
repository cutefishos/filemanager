import QtQuick 2.4
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0

import Qt.labs.platform 1.0
import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui

FolderViewDropArea {
    id: root
    visible: true

    preventStealing: true

    property bool containsDrag: false

    folderView: folderViewLayer.item

    function isDrag(fromX, fromY, toX, toY) {
        var length = Math.abs(fromX - toX) + Math.abs(fromY - toY);
        return length >= Qt.styleHints.startDragDistance;
    }

    function isFileDrag(event) {
        var taskUrl = event.mimeData.formats.indexOf("text/x-orgkdeplasmataskmanager_taskurl") !== -1;
        var arkService = event.mimeData.formats.indexOf("application/x-kde-ark-dndextract-service") !== -1;
        var arkPath = event.mimeData.formats.indexOf("application/x-kde-ark-dndextract-path") !== -1;
        return (event.mimeData.hasUrls || taskUrl || (arkService && arkPath));
    }

    onDragEnter: {
        if (!isFileDrag(event))
            event.ignore();

        // Firefox tabs are regular drags. Since all of our drop handling is asynchronous
        // we would accept this drop and have Firefox not spawn a new window. (Bug 337711)
        if (event.mimeData.formats.indexOf("application/x-moz-tabbrowser-tab") > -1) {
            event.ignore();
        }
    }

    onDragMove: {

    }

    onDragLeave: {

    }

    onDrop: {

    }

    DesktopSettings {
        id: settings
    }

    Image {
        id: wallpaper
        anchors.fill: parent
        source: "file://" + settings.wallpaper
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectCrop
        clip: true
        cache: false

        ColorOverlay {
            id: dimsWallpaper
            anchors.fill: wallpaper
            source: wallpaper
            color: "#000000"
            opacity: Meui.Theme.darkMode && settings.dimsWallpaper ? 0.4 : 0.0

            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }

        }
    }

    Loader {
        id: folderViewLayer
        anchors.fill: parent

        property bool ready: status == Loader.Ready
        property Item view: item ? item : null
        property QtObject model: item ? item.model : null

        focus: true
        active: true
        asynchronous: false
        source: "DesktopFolderView.qml"
    }
}
