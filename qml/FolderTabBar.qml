import QtQuick 2.12
import QtQuick.Controls 2.12
import Qt5Compat.GraphicalEffects
import FishUI 1.0 as FishUI

Rectangle {
    id: control
    property var tabs: []
    property int currentIndex: 0
    property int dragIndex: -1
    property int dropIndex: -1
    property real pointerX: 0
    property string dragTitle: ""
    signal selected(int index)
    signal closeRequested(int index)
    signal newRequested()
    signal duplicateRequested(string path)
    signal moveRequested(int from, int to)

    color: FishUI.Theme.secondBackgroundColor
    readonly property color separatorColor: Qt.rgba(FishUI.Theme.textColor.r,
        FishUI.Theme.textColor.g, FishUI.Theme.textColor.b, 0.10)
    readonly property color hoverColor: Qt.rgba(FishUI.Theme.textColor.r,
        FishUI.Theme.textColor.g, FishUI.Theme.textColor.b, 0.05)

    function updateDrop(x) {
        pointerX = x
        dropIndex = Math.max(0, Math.min(tabs.length - 1,
            Math.floor((x - list.x + list.contentX) / list.tabWidth)))
    }

    Rectangle {
        x: list.x
        y: list.y
        width: list.width
        height: list.height
        radius: height / 2
        color: FishUI.Theme.alternateBackgroundColor
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 40
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        orientation: ListView.Horizontal
        layoutDirection: Qt.LeftToRight
        clip: true
        model: control.tabs
        currentIndex: control.currentIndex
        readonly property real tabWidth: Math.max(140, width / Math.max(1, count))
        boundsBehavior: Flickable.StopAtBounds
        onCurrentIndexChanged: {
            if (control.dragIndex < 0)
                positionViewAtIndex(currentIndex, ListView.Contain)
        }
        onCountChanged: Qt.callLater(function() { list.positionViewAtIndex(list.currentIndex, ListView.Contain) })

        delegate: Item {
            id: tab
            objectName: "folderTab" + index
            width: list.tabWidth
            height: list.height
            property bool selected: index === control.currentIndex
            opacity: control.dragIndex === index ? 0.25 : 1

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: height / 2
                color: tab.selected ? (FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.secondBackgroundColor, 2)
                                                           : FishUI.Theme.secondBackgroundColor)
                                    : mouse.containsMouse ? control.hoverColor : "transparent"
                layer.enabled: tab.selected
                layer.effect: DropShadow {
                    transparentBorder: true
                    radius: 3
                    samples: 6
                    horizontalOffset: 0
                    verticalOffset: 1
                    color: Qt.rgba(0, 0, 0, FishUI.Theme.darkMode ? 0.18 : 0.12)
                }
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                preventStealing: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                property real pressX: 0
                property bool didDrag: false
                onPressed: function(event) {
                    didDrag = false
                    pressX = mapToItem(control, event.x, event.y).x
                    if (event.button === Qt.LeftButton)
                        control.selected(index)
                }
                onPositionChanged: function(event) {
                    if (!(pressedButtons & Qt.LeftButton))
                        return
                    var x = mapToItem(control, event.x, event.y).x
                    if (!didDrag && Math.abs(x - pressX) > 8) {
                        didDrag = true
                        control.dragIndex = index
                        control.dragTitle = modelData.tabTitle
                    }
                    if (didDrag)
                        control.updateDrop(x)
                }
                onReleased: {
                    if (didDrag) {
                        var from = control.dragIndex
                        var to = control.dropIndex
                        control.dragIndex = -1
                        control.dropIndex = -1
                        control.moveRequested(from, to)
                    }
                }
                onCanceled: {
                    control.dragIndex = -1
                    control.dropIndex = -1
                }
                onClicked: function(event) {
                    if (didDrag)
                        return
                    if (event.button === Qt.MiddleButton)
                        control.closeRequested(index)
                    else if (event.button === Qt.RightButton)
                        menu.popup()
                }
            }

            Label {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: modelData.tabTitle
                font.pixelSize: 12
                font.weight: tab.selected ? Font.Medium : Font.Normal
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
                opacity: tab.selected || mouse.containsMouse || hovered ? 1 : 0
                Accessible.name: qsTr("Close Tab")
                onClicked: control.closeRequested(index)
                contentItem: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        width: 10
                        height: 1.2
                        rotation: 45
                        color: FishUI.Theme.textColor
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 10
                        height: 1.2
                        rotation: -45
                        color: FishUI.Theme.textColor
                    }
                }
                background: Rectangle {
                    radius: height / 2
                    color: closeButton.down ? control.separatorColor
                                            : closeButton.hovered ? control.hoverColor : "transparent"
                }
            }

            FishUI.DesktopMenu {
                id: menu
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

    Rectangle {
        visible: control.dragIndex >= 0
        x: Math.max(list.x, Math.min(list.x + list.width - width, control.pointerX - width / 2))
        y: 4
        width: Math.min(list.tabWidth - 2, list.width)
        height: list.height - 2
        radius: height / 2
        color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.secondBackgroundColor, 2)
                                     : FishUI.Theme.secondBackgroundColor
        opacity: 0.95
        Label {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: control.dragTitle
            elide: Text.ElideMiddle
            color: FishUI.Theme.textColor
        }
    }

    Rectangle {
        visible: control.dragIndex >= 0 && control.dropIndex !== control.dragIndex
        x: Math.max(list.x, Math.min(list.x + list.width - width,
            list.x + (control.dropIndex + (control.dropIndex > control.dragIndex ? 1 : 0)) * list.tabWidth - list.contentX))
        y: 6
        width: 2
        height: control.height - 12
        radius: 1
        color: FishUI.Theme.highlightColor
    }

    Timer {
        interval: 30
        repeat: true
        running: control.dragIndex >= 0
        onTriggered: {
            var delta = control.pointerX < list.x + 24 ? -8
                      : control.pointerX > list.x + list.width - 24 ? 8 : 0
            list.contentX = Math.max(0, Math.min(Math.max(0, list.contentWidth - list.width), list.contentX + delta))
            control.updateDrop(control.pointerX)
        }
    }

    ToolButton {
        id: addButton
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 28
        text: "+"
        font.pixelSize: 20
        Accessible.name: qsTr("New Tab")
        onClicked: control.newRequested()
        background: Rectangle {
            radius: height / 2
            color: addButton.hovered ? control.hoverColor : "transparent"
        }
    }
}
