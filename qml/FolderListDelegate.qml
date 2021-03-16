import QtQuick 2.4
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    id: control

    property int index: model.index
    property string name: model.blank ? "" : model.display
    property bool blank: model.blank
    property bool isDir: model.blank ? false : model.isDir
    property bool selected: model.blank ? false : model.selected
    property Item frame: contentItem
    property Item iconArea: iconItem
    property Item labelArea: label1

    property color hoveredColor: Qt.rgba(Meui.Theme.textColor.r,
                                         Meui.Theme.textColor.g,
                                         Meui.Theme.textColor.b, 0.1)

    Accessible.name: name
    Accessible.role: Accessible.Canvas

    MouseArea {
        id: _mouseArea
        anchors.fill: parent
        hoverEnabled: true
    }

    Rectangle {
        z: -1
        anchors.fill: parent
        radius: Meui.Theme.bigRadius
        color: selected ? Meui.Theme.highlightColor : _mouseArea.containsMouse ? control.hoveredColor : "transparent"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Meui.Units.smallSpacing
        anchors.rightMargin: Meui.Units.smallSpacing
        spacing: Meui.Units.largeSpacing

        Item {
            id: iconItem
            Layout.fillHeight: true
            width: parent.height * 0.8

            PlasmaCore.IconItem {
                id: icon
                z: 2

                anchors.fill: parent

                animated: false
                usesPlasmaTheme: false
                smooth: true
                source: model.blank ? "" : model.decoration
                overlays: model.blank ? "" : model.overlays
            }
        }

        Label {
            id: label1
            Layout.fillWidth: true
            text: name
            color: selected ? Meui.Theme.highlightedTextColor : Meui.Theme.textColor
        }

        Label {
            id: label2
            color: selected ? Meui.Theme.highlightedTextColor : Meui.Theme.textColor
        }
    }
}
