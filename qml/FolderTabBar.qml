import QtQuick 2.12
import QtQuick.Controls 2.12
import FishUI 1.0 as FishUI

Rectangle {
    id: control
    property var tabs: []
    property int currentIndex: 0
    // Model index of the tab being dragged, and the slot it currently occupies.
    property int dragIndex: -1
    property int dropIndex: -1
    property real pointerX: 0
    property real dragPos: 0
    property real dragOffset: 0
    property int springLoadIndex: -1
    signal selected(int index)
    signal closeRequested(int index)
    signal newRequested()
    signal duplicateRequested(string path)
    signal moveRequested(int from, int to)
    // `position` is the pointer, in the window.
    signal detachRequested(int index, point position)

    // How far above or below the bar a dragged tab comes off it into a window of its own.
    readonly property real detachDistance: 16

    color: FishUI.Theme.secondBackgroundColor
    readonly property color pressedColor: Qt.rgba(FishUI.Theme.textColor.r,
        FishUI.Theme.textColor.g, FishUI.Theme.textColor.b, 0.10)
    readonly property color hoverColor: Qt.rgba(FishUI.Theme.textColor.r,
        FishUI.Theme.textColor.g, FishUI.Theme.textColor.b, 0.05)

    // Where a tab sits while a drag is in progress: the dragged one is taken
    // out of the order and put back at dropIndex, so the rest slide aside.
    function slotFor(index) {
        if (dragIndex < 0 || index === dragIndex)
            return index
        var without = index - (index > dragIndex ? 1 : 0)
        return without + (without >= dropIndex ? 1 : 0)
    }

    function updateDrag(x) {
        pointerX = x
        var limit = Math.max(0, list.contentWidth - list.tabWidth)
        dragPos = Math.max(0, Math.min(limit, x - list.x + list.contentX - dragOffset))
        dropIndex = Math.max(0, Math.min(tabs.length - 1, Math.round(dragPos / list.tabWidth)))
    }

    // The slot a tab dropped at `x` goes into.
    function insertionIndexAt(x) {
        return Math.max(0, Math.min(tabs.length, Math.round((x - list.x + list.contentX) / list.tabWidth)))
    }

    function indexAtPosition(x, y) {
        if (x < list.x || x >= list.x + list.width || y < list.y || y >= list.y + list.height)
            return -1
        return Math.max(0, Math.min(tabs.length - 1,
                                   Math.floor((x - list.x + list.contentX) / list.tabWidth)))
    }

    FishUI.RoundedRectangle {
        x: list.x
        y: list.y
        width: list.width
        height: list.height
        radius: height / 2
        continuousCorners: false
        color: FishUI.Theme.alternateBackgroundColor
    }

    Flickable {
        id: list
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 40
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        clip: true
        interactive: control.dragIndex < 0
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: tabWidth * count
        contentHeight: height

        readonly property int count: control.tabs.length
        readonly property real tabWidth: Math.max(140, width / Math.max(1, count))

        function ensureVisible(index) {
            if (index < 0 || index >= count)
                return
            var left = index * tabWidth
            var limit = Math.max(0, contentWidth - width)
            contentX = Math.max(0, Math.min(limit,
                left < contentX ? left
                                : left + tabWidth > contentX + width ? left + tabWidth - width
                                                                     : contentX))
        }

        Repeater {
            model: control.tabs

            delegate: Item {
                id: tab
                objectName: "folderTab" + index
                width: list.tabWidth
                height: list.height
                x: control.dragIndex === index ? control.dragPos
                                               : control.slotFor(index) * list.tabWidth
                z: control.dragIndex === index ? 2 : tab.selected ? 1 : 0
                property bool selected: index === control.currentIndex
                property bool springLoadTarget: index === control.springLoadIndex

                // Only the shuffling aside during a drag animates; opening or
                // closing a tab must place the rest at once.
                Behavior on x {
                    enabled: control.dragIndex >= 0 && control.dragIndex !== index
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: height / 2
                    color: tab.selected ? (FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.secondBackgroundColor, 2)
                                                               : FishUI.Theme.secondBackgroundColor)
                                        : mouse.containsMouse || tab.springLoadTarget
                                                ? control.hoverColor : "transparent"
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    preventStealing: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                    property real pressX: 0
                    property real pressY: 0
                    property bool didDrag: false
                    // Off the bar: the rest of the press belongs to the window drag.
                    property bool detached: false
                    onPressed: function(event) {
                        didDrag = false
                        detached = false
                        pressX = mapToItem(control, event.x, event.y).x
                        pressY = mapToItem(control, event.x, event.y).y
                        if (event.button === Qt.LeftButton) {
                            control.dragOffset = event.x
                            control.selected(index)
                        }
                    }
                    onPositionChanged: function(event) {
                        if (!(pressedButtons & Qt.LeftButton) || detached)
                            return
                        var position = mapToItem(control, event.x, event.y)
                        if (!didDrag && (Math.abs(position.x - pressX) > 8 || Math.abs(position.y - pressY) > 8)) {
                            didDrag = true
                            control.dragIndex = index
                            control.dropIndex = index
                        }
                        if (didDrag && (position.y < -control.detachDistance
                                        || position.y > control.height + control.detachDistance)) {
                            didDrag = false
                            detached = true
                            control.dragIndex = -1
                            control.dropIndex = -1
                            control.detachRequested(index, mapToItem(null, event.x, event.y))
                            return
                        }
                        if (didDrag)
                            control.updateDrag(position.x)
                    }
                    onReleased: {
                        if (!didDrag)
                            return
                        var from = control.dragIndex
                        var to = control.dropIndex
                        // Clear first: the tabs are already in their final
                        // places, so the reorder must not animate them again.
                        control.dragIndex = -1
                        control.dropIndex = -1
                        control.moveRequested(from, to)
                    }
                    onCanceled: {
                        control.dragIndex = -1
                        control.dropIndex = -1
                    }
                    onClicked: function(event) {
                        if (didDrag || detached)
                            return
                        if (event.button === Qt.MiddleButton)
                            control.closeRequested(index)
                        else if (event.button === Qt.RightButton)
                            menu.popup()
                    }
                }

                FishUI.Label {
                    anchors.fill: parent
                    anchors.leftMargin: 28
                    anchors.rightMargin: 28
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: modelData.tabTitle
                    font: Qt.font({ family: FishUI.Theme.fontFamily, pixelSize: 12,
                                    weight: tab.selected ? Font.Medium : Font.Normal })
                    elide: Text.ElideMiddle
                    color: FishUI.Theme.textColor
                }

                ToolButton {
                    id: closeButton
                    objectName: "tabCloseButton"
                    width: 20
                    height: 20
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    visible: control.dragIndex < 0
                    opacity: tab.selected || mouse.containsMouse || hovered ? 1 : 0
                    Accessible.name: qsTr("Close Tab")
                    onClicked: control.closeRequested(index)
                    contentItem: Item {
                        // The window buttons' artwork, at its own 24px grid.
                        Image {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            sourceSize: Qt.size(24, 24)
                            source: "qrc:/fishui/kit/images/" + (FishUI.Theme.darkMode ? "dark/" : "light/") + "close.svg"
                            smooth: false
                            antialiasing: true
                        }
                    }
                    background: Rectangle {
                        radius: height / 2
                        color: closeButton.down ? control.pressedColor
                                                : closeButton.hovered ? control.hoverColor : "transparent"
                    }
                }

                FishUI.DesktopMenu {
                    id: menu
                    pointerGrabEnabled: true

                    FishUI.MenuItem {
                        text: qsTr("Open In New Tab")
                        onTriggered: control.duplicateRequested(modelData.currentUrl)
                    }
                    FishUI.MenuItem {
                        text: qsTr("Close Tab")
                        onTriggered: control.closeRequested(index)
                    }
                }
            }
        }
    }

    onCurrentIndexChanged: {
        if (control.dragIndex < 0)
            list.ensureVisible(control.currentIndex)
    }
    onTabsChanged: Qt.callLater(function() { list.ensureVisible(control.currentIndex) })

    Timer {
        interval: 30
        repeat: true
        running: control.dragIndex >= 0
        onTriggered: {
            var delta = control.pointerX < list.x + 24 ? -8
                      : control.pointerX > list.x + list.width - 24 ? 8 : 0
            if (delta === 0)
                return
            list.contentX = Math.max(0, Math.min(Math.max(0, list.contentWidth - list.width),
                                                 list.contentX + delta))
            control.updateDrag(control.pointerX)
        }
    }

    FishUI.HeaderButton {
        id: addButton
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        size: 28
        artwork: "add"
        hoveredColor: control.hoverColor
        pressedColor: control.pressedColor
        Accessible.name: qsTr("New Tab")
        onClicked: control.newRequested()
    }
}
