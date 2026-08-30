/*
 * Copyright (C) 2021 CutefishOS Team.
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

// Entry point of the desktop, loaded by the DesktopView item. The primary
// screen gets the full desktop (wallpaper + icons); the others get the
// wallpaper on its own.
Loader {
    id: root

    asynchronous: false
    source: desktopView.primary ? "Main.qml" : "Wallpaper.qml"

    onLoaded: {
        item.width = Qt.binding(function() { return root.width })
        item.height = Qt.binding(function() { return root.height })
    }
}
