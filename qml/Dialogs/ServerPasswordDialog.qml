// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import FishUI 1.0 as FishUI

FishUI.DialogWindow {
    id: control

    property var placesModel
    property string address
    property string user
    property string domain
    property bool needsDomain: false
    property bool anonymousAllowed: false
    property bool retry: false

    property bool _answered: false
    readonly property bool _guest: anonymousAllowed && guestButton.checked
    readonly property bool _complete: _guest
                                      || (userField.text.length > 0 && passwordField.text.length > 0)

    title: qsTr("Connect to Server")
    width: 400
    contentHeight: layout.implicitHeight + 32
    background.color: FishUI.Theme.secondBackgroundColor
    // The background item reaches a grown window a frame or two after its buffer does.
    color: background.color

    // Created per request; closing without an answer gives the connection up.
    onClosing: {
        if (!_answered)
            placesModel.cancelConnection(address)
        destroy()
    }

    Component.onCompleted: {
        open()
        if (userField.text.length === 0)
            userField.forceActiveFocus()
        else
            passwordField.forceActiveFocus()
    }

    function accept() {
        if (!_complete)
            return
        _answered = true
        placesModel.answerPassword(address, _guest ? "" : userField.text,
                                   _guest ? "" : domainField.text,
                                   _guest ? "" : passwordField.text,
                                   _guest, !_guest && rememberBox.checked)
        close()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: control.close()
    }

    ColumnLayout {
        id: layout
        // Not filled to the window: the window height follows this layout's implicit
        // height a polish later, and a filled layout would be spread over the old height.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12

        FishUI.Label {
            Layout.fillWidth: true
            text: control.retry ? qsTr("The name or password for “%1” is incorrect. Try again.")
                                        .arg(control.address)
                                : qsTr("Enter your name and password for “%1”.").arg(control.address)
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: FishUI.Theme.disabledTextColor
        }

        RowLayout {
            visible: control.anonymousAllowed
            spacing: 16

            FishUI.RadioButton {
                id: userButton
                text: qsTr("Registered user")
                checked: true
            }

            FishUI.RadioButton {
                id: guestButton
                text: qsTr("Guest")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            enabled: !control._guest
            opacity: enabled ? 1 : 0.5

            FishUI.TextField {
                id: userField
                Layout.fillWidth: true
                text: control.user
                placeholderText: qsTr("Name")
                selectByMouse: true
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onAccepted: passwordField.forceActiveFocus()
            }

            FishUI.TextField {
                id: domainField
                Layout.fillWidth: true
                visible: control.needsDomain
                text: control.domain
                placeholderText: qsTr("Domain (optional)")
                selectByMouse: true
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onAccepted: passwordField.forceActiveFocus()
            }

            FishUI.TextField {
                id: passwordField
                Layout.fillWidth: true
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
                selectByMouse: true
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                onAccepted: control.accept()
            }

            FishUI.CheckBox {
                id: rememberBox
                text: qsTr("Remember this password")
                checked: true
            }
        }

        RowLayout {
            Layout.topMargin: 4
            Layout.alignment: Qt.AlignRight
            spacing: 8

            FishUI.ConfirmDialogButton {
                text: qsTr("Cancel")
                onClicked: control.close()
            }

            FishUI.ConfirmDialogButton {
                text: qsTr("Connect")
                primary: true
                enabled: control._complete
                onClicked: control.accept()
            }
        }
    }
}
