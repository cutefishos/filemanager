import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import MeuiKit 1.0 as Meui
import Cutefish.FileManager 1.0 as FM

ListView {
    id: control

    signal clicked(var mouse)
    signal positionChanged(var mouse)
    signal pressed(var mouse)
    signal released(var mouse)

    ScrollBar.vertical: ScrollBar {}
    spacing: Meui.Units.largeSpacing
    clip: true

    snapMode: ListView.NoSnap

    highlightFollowsCurrentItem: true
    highlightMoveDuration: 0
    highlightResizeDuration : 0

    MouseArea {
        anchors.fill: parent
        z: -1
        hoverEnabled: true
        propagateComposedEvents: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: parent.clicked(mouse)
        onPositionChanged: parent.positionChanged(mouse)
        onPressed: parent.pressed(mouse)
        onReleased: parent.released(mouse)
        Keys.forwardTo: _listViewBrowser
    }
}
