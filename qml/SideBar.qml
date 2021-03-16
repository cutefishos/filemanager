import QtQuick 2.4
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4
import MeuiKit 1.0 as Meui
import Cutefish.FileManager 1.0

ListView {
    id: control
    implicitWidth: 200

    property string currentUrl

    signal placeClicked(string path)
    signal itemClicked(int index)

    onItemClicked: {
        var item = placesModel.get(index)
        control.placeClicked(item.url)
    }

    onCurrentUrlChanged: {
        syncIndex(currentUrl)
    }

    Component.onCompleted: {
        syncIndex(currentUrl)
    }

    function syncIndex(path) {
        control.currentIndex = -1

        for (var i = 0; i < control.count; ++i) {
            if (path === control.model.get(i).url) {
                control.currentIndex = i
                break
            }
        }
    }

    PlacesModel {
        id: placesModel
    }

    clip: true
    spacing: Meui.Units.smallSpacing
    leftMargin: Meui.Units.smallSpacing
    rightMargin: Meui.Units.smallSpacing

    model: placesModel

    ScrollBar.vertical: ScrollBar {}
    flickableDirection: Flickable.VerticalFlick

    highlightFollowsCurrentItem: true
    highlightMoveDuration: 0
    highlightResizeDuration : 0

    highlight: Rectangle {
        radius: Meui.Theme.smallRadius
        color: Meui.Theme.highlightColor
    }

    delegate: SidebarItem {
        id: listItem
        checked: control.currentIndex === index
        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
        height: 40

        onClicked: {
            control.currentIndex = index
            control.itemClicked(index)
        }
    }
}
