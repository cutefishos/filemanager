import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.0

import Cutefish.FileManager 1.0
import MeuiKit 1.0 as Meui

Item {
    id: control

    width: GridView.view.cellWidth
    height: GridView.view.cellHeight

    property Item iconArea: _image.visible ? _image : _icon
    property Item textArea: _label

    property int index: model.index
    property bool hovered: GridView.view.hoveredItem === control
    property bool selected: model.selected
    property bool blank: model.blank
    property var fileName: model.fileName

    property color hoveredColor: Meui.Theme.darkMode ? Qt.lighter(Meui.Theme.backgroundColor, 1.1)
                                                     : Qt.darker(Meui.Theme.backgroundColor, 1.05)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Meui.Units.largeSpacing
        spacing: Meui.Units.smallSpacing

        Item {
            id: _iconItem
            Layout.preferredHeight: parent.height * 0.6
            Layout.fillWidth: true

            Image {
                id: _icon
                anchors.centerIn: parent
                width: parent.height
                height: width
                sourceSize: Qt.size(width, height)
                source: "image://icontheme/" + model.iconName
                visible: !_image.visible
            }

            Image {
                id: _image
                anchors.centerIn: parent
                height: parent.height
                width: parent.width

                fillMode: Image.PreserveAspectFit
                visible: status === Image.Ready
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                sourceSize.width: width
                sourceSize.height: height
                source: model.thumbnail ? model.thumbnail : ""
                asynchronous: true
                cache: true

                layer.enabled: true
                layer.effect: OpacityMask {
                    maskSource: Item {
                        width: _image.width
                        height: _image.height

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.min(parent.width, _image.paintedWidth)
                            height: Math.min(parent.height, _image.paintedHeight)
                            radius: Meui.Theme.smallRadius
                        }
                    }
                }
            }
        }

        Item {
            id: _labelItem
            Layout.fillHeight: true
            Layout.preferredHeight: Math.min(_label.implicitHeight, height)
            Layout.fillWidth: true

            Rectangle {
                width: _label.paintedWidth + Meui.Units.smallSpacing * 2
                height: _label.paintedHeight
                x: (_labelItem.width - width + Meui.Units.smallSpacing) / 2
                color: selected ? Meui.Theme.highlightColor : hovered ? hoveredColor : "transparent"
                radius: Meui.Theme.smallRadius
            }

            Label {
                id: _label
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignTop
                anchors.fill: parent
                anchors.margins: 0
                elide: Qt.ElideRight
                wrapMode: Text.Wrap
                text: model.fileName
                color: selected ? Meui.Theme.highlightedTextColor : Meui.Theme.textColor
            }
        }
    }
}
