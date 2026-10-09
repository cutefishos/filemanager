// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

FishUI.ConfirmDialog {
    required property string path
    required property string name

    title: qsTr("Run Program")
    text: qsTr("“%1” is a program. Only run it if you trust where it came from.").arg(name)
    cancelText: qsTr("Cancel")
    confirmText: qsTr("Run")

    onAccepted: FileLauncher.trustAndLaunch(path)
}
