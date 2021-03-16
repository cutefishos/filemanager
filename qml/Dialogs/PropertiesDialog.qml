import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import MeuiKit 1.0 as Meui

Window {
    id: control
    title: qsTr("Properties")
    flags: Qt.Dialog | Qt.WindowStaysOnTopHint

    visible: true

    onVisibleChanged: {
        if (visible) updateWindowSize()
    }

    function updateWindowSize() {
        if (visible) {
            control.width = _mainLayout.implicitWidth + _mainLayout.anchors.leftMargin + _mainLayout.anchors.rightMargin
            control.height = _mainLayout.implicitHeight + _mainLayout.anchors.topMargin + _mainLayout.anchors.bottomMargin
            control.minimumWidth = control.width
            control.minimumHeight = control.height
            control.maximumWidth = control.width
            control.maximumHeight = control.height

            if (_textField.enabled)
                _textField.forceActiveFocus()
        }
    }

    Item {
        id: _contentItem
        anchors.fill: parent
        focus: true

        Keys.enabled: true
        Keys.onEscapePressed: control.close()

        ColumnLayout {
            id: _mainLayout
            anchors.fill: parent
            anchors.leftMargin: Meui.Units.largeSpacing * 2
            anchors.rightMargin: Meui.Units.largeSpacing * 2
            anchors.topMargin: Meui.Units.largeSpacing
            anchors.bottomMargin: Meui.Units.largeSpacing
            spacing: Meui.Units.largeSpacing

            RowLayout {
                spacing: Meui.Units.largeSpacing * 2

                Image {
                    width: 64
                    height: width
                    sourceSize: Qt.size(width, height)
                    source: "image://icontheme/" + main.iconName
                }

                TextField {
                    id: _textField
                    text: main.fileName
                    focus: true
                    Layout.fillWidth: true
                    Keys.onEscapePressed: control.close()
                    enabled: !main.multiple
                }
            }

            GridLayout {
                columns: 2
                columnSpacing: Meui.Units.largeSpacing
                rowSpacing: Meui.Units.largeSpacing
                Layout.alignment: Qt.AlignTop

                onHeightChanged: updateWindowSize()
                onImplicitHeightChanged: updateWindowSize()

                Label {
                    text: qsTr("Type:")
                    Layout.alignment: Qt.AlignRight
                    visible: mimeType.visible
                }

                Label {
                    id: mimeType
                    text: main.mimeType
                    visible: text
                }

                Label {
                    text: qsTr("Location:")
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    id: location
                    text: main.location
                }

                Label {
                    text: qsTr("Size:")
                    Layout.alignment: Qt.AlignRight
                    visible: size.visible
                }

                Label {
                    id: size
                    text: main.size
                    visible: text
                }

                Label {
                    text: qsTr("Created:")
                    Layout.alignment: Qt.AlignRight
                    visible: creationTime.visible
                }

                Label {
                    id: creationTime
                    text: main.creationTime
                    visible: text
                }

                Label {
                    text: qsTr("Modified:")
                    Layout.alignment: Qt.AlignRight
                    visible: modifiedTime.visible
                }

                Label {
                    id: modifiedTime
                    text: main.modifiedTime
                    visible: text
                }

                Label {
                    text: qsTr("Accessed:")
                    Layout.alignment: Qt.AlignRight
                    visible: accessTime.visible
                }

                Label {
                    id: accessTime
                    text: main.accessedTime
                    visible: text
                }
            }

            Item {
                height: Meui.Units.largeSpacing
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Meui.Units.largeSpacing

                Button {
                    text: qsTr("Cancel")
                    Layout.fillWidth: true
                    onClicked: control.close()
                }

                Button {
                    text: qsTr("OK")
                    Layout.fillWidth: true
                    onClicked: control.close()
                }
            }
        }
    }
}
