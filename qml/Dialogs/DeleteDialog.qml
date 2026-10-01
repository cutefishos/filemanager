// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import FishUI 1.0 as FishUI

FishUI.ConfirmDialog {
    // Guarded: the folder may close while the question is open.
    required property QtObject folderModel

    title: qsTr("Delete")
    text: qsTr("Do you want to delete it permanently?")
    cancelText: qsTr("Cancel")
    confirmText: qsTr("Delete")
    destructive: true

    onAccepted: {
        if (folderModel)
            folderModel.deleteSelected()
    }
}
