import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

import FishUI 1.0 as FishUI

FishUI.Window {
    id: control

    property QtObject archiveModel
    // Not `Window`: that is QtQuick.Window's own type, and the desktop's host
    // window is a QQuickWindow subclass from C++, which is not one.
    property QtObject hostWindow

    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    modality: Qt.NonModal
    minimizeButtonVisible: false
    visible: archiveModel !== null && archiveModel.archiveBusy

    width: 420
    height: 178
    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height

    x: hostWindow ? hostWindow.x + Math.round((hostWindow.width - width) / 2) : 0
    y: hostWindow ? hostWindow.y + Math.round((hostWindow.height - height) / 2) : 0

    header.height: 40
    headerBackground.color: FishUI.Theme.secondBackgroundColor
    background.color: FishUI.Theme.secondBackgroundColor

    headerItem: Item {
        Label {
            anchors.left: parent.left
            anchors.leftMargin: FishUI.Units.largeSpacing
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - FishUI.Units.largeSpacing * 2
            text: control.archiveModel === null ? qsTr("File operation")
                                                 : control.archiveModel.archiveOperation
            elide: Text.ElideRight
            font.pointSize: 11
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.largeSpacing
        anchors.rightMargin: FishUI.Units.largeSpacing
        anchors.topMargin: FishUI.Units.smallSpacing
        anchors.bottomMargin: FishUI.Units.largeSpacing
        spacing: FishUI.Units.largeSpacing

        Label {
            Layout.fillWidth: true
            text: control.archiveModel === null ? ""
                                                  : control.archiveModel.archiveCurrentFile
            visible: text.length > 0
            elide: Text.ElideMiddle
            font.pointSize: 10
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: 20

            FishUI.BusyIndicator {
                anchors.centerIn: parent
                width: 20
                height: 20
                visible: control.archiveModel !== null
                         && control.archiveModel.archiveProgress < 0
                running: visible
            }

            ProgressBar {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: control.archiveModel !== null
                         && control.archiveModel.archiveProgress >= 0
                from: 0
                to: 100
                value: control.archiveModel === null ? 0
                                                      : control.archiveModel.archiveProgress
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: control.archiveModel !== null && control.archiveModel.archiveCancelling
                      ? qsTr("Cancelling…") : qsTr("Cancel")
                enabled: control.archiveModel !== null
                         && control.archiveModel.archiveBusy
                         && !control.archiveModel.archiveCancelling
                flat: true
                onClicked: control.archiveModel.cancelArchive()
            }
        }
    }

    onClosing: function(closeEvent) {
        if (control.archiveModel !== null && control.archiveModel.archiveBusy) {
            control.archiveModel.cancelArchive()
            closeEvent.accepted = false
        }
    }

    function updateTransientParent() {
        if (hostWindow)
            control.transientParent = hostWindow
    }

    onHostWindowChanged: updateTransientParent()

    Component.onCompleted: updateTransientParent()

    onVisibleChanged: {
        if (visible)
            requestActivate()
    }
}
