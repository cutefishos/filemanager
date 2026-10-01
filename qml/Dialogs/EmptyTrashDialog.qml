// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

 
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0
import Cutefish.Application

FishUI.ConfirmDialog {
    // Opened from the dock it has no file manager window to belong to, so it
    // would otherwise show up as an application of its own and sink below others.
    WindowHints.skipTaskbar: true
    WindowHints.alwaysOnTop: true

    title: qsTr("Empty Trash")
    text: qsTr("Do you want to permanently delete all files from the Trash?")
    cancelText: qsTr("Cancel")
    confirmText: qsTr("Empty Trash")
    destructive: true

    Fm {
        id: fm
    }

    onAccepted: fm.emptyTrash()
}
