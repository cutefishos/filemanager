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
import FishUI 1.0 as FishUI

FishUI.HeaderButton {
    iconSize: 18
    // Translucent, so the buttons sit on the frosted header without a solid
    // patch. Over the plain window background they match the old opaque tones.
    backgroundColor: darkMode ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(1, 1, 1, 0.6)
    hoveredColor: Qt.rgba(tint.r, tint.g, tint.b, darkMode ? 0.21 : 0.17)
    pressedColor: Qt.rgba(tint.r, tint.g, tint.b, darkMode ? 0.15 : 0.26)
}
