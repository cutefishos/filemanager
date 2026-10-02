// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

FishUI.ConfirmDialog {
    required property string path
    required property string name
    required property bool desktopFile

    title: desktopFile ? qsTr("Untrusted Launcher") : qsTr("Untrusted Program")
    text: desktopFile
          ? qsTr("“%1” is an application launcher that is not marked as trusted. Launching it runs the command it contains. Only launch it if you trust where it came from.").arg(name)
          : qsTr("“%1” is a program that is not marked as executable. Only run it if you trust where it came from.").arg(name)
    cancelText: qsTr("Cancel")
    confirmText: desktopFile ? qsTr("Trust and Launch") : qsTr("Run")

    onAccepted: FileLauncher.trustAndLaunch(path)
}
