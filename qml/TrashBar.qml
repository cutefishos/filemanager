// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Layouts 1.12
import FishUI 1.0 as FishUI

Item {
    id: control

    property var folderModel: null

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.smallSpacing * 1.5
        anchors.rightMargin: FishUI.Units.largeSpacing
        spacing: FishUI.Units.smallSpacing

        FishUI.Label {
            text: qsTr("Trash")
            font.weight: Font.DemiBold
            Layout.leftMargin: FishUI.Units.smallSpacing
        }

        Item {
            Layout.fillWidth: true
        }

        FishUI.Button {
            objectName: "emptyTrashButton"
            text: qsTr("Empty")
            implicitHeight: 24
            topPadding: 0
            bottomPadding: 0
            enabled: control.folderModel && control.folderModel.count > 0
            onClicked: control.folderModel.emptyTrash()
        }
    }
}
