import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import MeuiKit 1.0 as Meui

Window {
    id: control

    title: qsTr("New Folder")
    flags: Qt.Dialog
    visible: true

    width: 400 + Meui.Units.largeSpacing * 2
    height: _mainLayout.implicitHeight + Meui.Units.largeSpacing * 4

    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height

    Rectangle {
        anchors.fill: parent
        color: Meui.Theme.backgroundColor
    }

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.leftMargin: Meui.Units.largeSpacing
        anchors.rightMargin: Meui.Units.largeSpacing
        spacing: 0

        RowLayout {
            Label {
                text: qsTr("Name")
            }

            TextField {
                id: _textField
                Layout.fillWidth: true
                Keys.onEscapePressed: control.close()
                focus: true
            }
        }

        RowLayout {
            Button {
                text: qsTr("Cancel")
                Layout.fillWidth: true
                onClicked: control.close()
            }

            Button {
                text: qsTr("OK")
                Layout.fillWidth: true
                onClicked: {
                    main.newFolder(_textField.text)
                    control.close()
                }
                enabled: _textField.text
                flat: true
            }
        }
    }
}
