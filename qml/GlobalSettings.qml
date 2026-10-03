// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtCore

Settings {
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
