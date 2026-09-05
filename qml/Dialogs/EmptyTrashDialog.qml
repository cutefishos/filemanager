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
 
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

FishUI.ConfirmDialog {
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
