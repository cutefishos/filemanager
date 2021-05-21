import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

Window {
    id: control

    title: qsTr("File Manager")
    flags: Qt.Dialog
    visible: true

    width: 300 + FishUI.Units.largeSpacing * 2
    height: _mainLayout.implicitHeight + FishUI.Units.largeSpacing * 3

    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height

    Fm {
        id: fm
    }

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.backgroundColor
    }

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.largeSpacing
        anchors.rightMargin: FishUI.Units.largeSpacing
        anchors.bottomMargin: FishUI.Units.smallSpacing
        spacing: FishUI.Units.largeSpacing

        Label {
            text: qsTr("Do you want to permanently delete all files from the Trash?")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        RowLayout {
            spacing: FishUI.Units.largeSpacing

            Button {
                text: qsTr("Cancel")
                Layout.fillWidth: true
                onClicked: control.close()
            }

            Button {
                text: qsTr("Empty Trash")
                focus: true
                Layout.fillWidth: true
                onClicked: {
                    fm.emptyTrash()
                    control.close()
                }
                flat: true
            }
        }
    }
}
