/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     revenmartin <revenmartin@gmail.com>
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

Window {
    id: control

    title: qsTr("File Manager")
    flags: Qt.Dialog
    visible: true

    width: 300 + FishUI.Units.largeSpacing * 2
    height: _mainLayout.implicitHeight + FishUI.Units.largeSpacing * 2

    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height

    Fm {
        id: fm
    }

    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.secondBackgroundColor
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
