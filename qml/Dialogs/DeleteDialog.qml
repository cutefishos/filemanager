/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

FishUI.Window {
    id: control

    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    minimizeButtonVisible: false
    visible: true

    width: contentWidth
    height: contentHeight

    minimumWidth: contentWidth
    minimumHeight: contentHeight
    maximumWidth: contentWidth
    maximumHeight: contentHeight

    property var contentWidth: _mainLayout.implicitWidth + FishUI.Units.largeSpacing * 2
    property var contentHeight: _mainLayout.implicitHeight + control.header.height + FishUI.Units.largeSpacing * 2

    background.color: FishUI.Theme.secondBackgroundColor

    ColumnLayout {
        id: _mainLayout
        anchors.fill: parent
        anchors.leftMargin: FishUI.Units.largeSpacing
        anchors.rightMargin: FishUI.Units.largeSpacing
        anchors.bottomMargin: FishUI.Units.smallSpacing
        spacing: FishUI.Units.largeSpacing

        Label {
            text: qsTr("Do you want to delete it permanently?")
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
                text: qsTr("Delete")
                focus: true
                Layout.fillWidth: true
                onClicked: {
                    control.close()
                    model.deleteSelected()
                }
                flat: true
            }
        }
    }
}
