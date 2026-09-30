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
import QtCore

Settings {
    // Pinned: the file chooser, a process of its own, shares the file manager's preferences.
    location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation)
              + "/cutefish/filemanager.conf"

    property int viewMethod: 1          // controls display mode: list or grid

    // Port to C++
    // property bool showHiddenFiles: false

    // Name, Date, Size
    property int orderBy: 0
    property int sortMode: 0
    property bool sortDesc: false

    property real listDateColumnWidth: 180
    property real listSizeColumnWidth: 96
    property real listTypeColumnWidth: 150

    // UI
    property int maximumIconSize: 256
    property int minimumIconSize: 64

    property int gridIconSize: 64

    // Type sorting used to be stored as 6, which is not a model column.
    Component.onCompleted: {
        if (sortMode === 6)
            sortMode = 3
    }
}
