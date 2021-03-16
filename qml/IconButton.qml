import QtQuick 2.12
import MeuiKit 1.0 as Meui

Item {
    id: control
    width: 24
    height: 24

    property alias source: _image.source
    property color hoveredColor: Meui.Theme.darkMode ? Qt.lighter(Meui.Theme.backgroundColor, 1.1)
                                                   : Qt.darker(Meui.Theme.backgroundColor, 1.2)
    property color pressedColor: Meui.Theme.darkMode ? Qt.lighter(Meui.Theme.backgroundColor, 1.2)
                                                     : Qt.darker(Meui.Theme.backgroundColor, 1.3)

    signal clicked()

    Rectangle {
        id: _background
        anchors.fill: parent
        radius: Meui.Theme.smallRadius
        color: _mouseArea.pressed ? pressedColor : _mouseArea.containsMouse ? control.hoveredColor : Meui.Theme.backgroundColor
    }

    Image {
        id: _image
        anchors.centerIn: parent
        width: control.height * 0.64
        height: width
        sourceSize: Qt.size(width, height)
    }

    MouseArea {
        id: _mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: control.clicked()
    }
}
