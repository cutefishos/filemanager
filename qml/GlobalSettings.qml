import QtQuick 2.12
import Qt.labs.settings 1.0

Settings {
    property int viewMethod: 1          // controls display mode: list or grid
    property bool showHidden: false

    // Name, Date, Size
    property int orderBy: 0

    // UI
    property int width: 900
    property int height: 580
    property int desktopIconSize: 72
    property int maximumIconSize: 256
    property int minimumIconSize: 64

    property int gridIconSize: 64
}
