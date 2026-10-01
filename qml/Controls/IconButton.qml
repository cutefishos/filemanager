// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

 
import QtQuick 2.12
import FishUI 1.0 as FishUI

FishUI.HeaderButton {
    iconSize: 18
    hoveredColor: Qt.rgba(tint.r, tint.g, tint.b, darkMode ? 0.21 : 0.17)
    pressedColor: Qt.rgba(tint.r, tint.g, tint.b, darkMode ? 0.15 : 0.26)
}
