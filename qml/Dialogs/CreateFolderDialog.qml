import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import FishUI 1.0 as FishUI

Window {
    id: control

    title: qsTr("New folder name")
    flags: Qt.Dialog
    visible: true

    width: 400 + FishUI.Units.largeSpacing * 2
    height: _mainLayout.implicitHeight + FishUI.Units.largeSpacing * 2

    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.backgroundColor
    }

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.largeSpacing
        anchors.rightMargin: FishUI.Units.largeSpacing
        spacing: 0

        TextField {
            id: _textField
            Layout.fillWidth: true
            Keys.onEscapePressed: control.close()
            text: qsTr("New folder")
            focus: true

            onAccepted: {
                main.newFolder(_textField.text)
                control.close()
            }

            Component.onCompleted: {
                _textField.selectAll()
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
