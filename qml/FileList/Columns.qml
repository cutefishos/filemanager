import QtQuick
import FishUI 1.0 as FishUI

// Column layout and sort state shared by every file list, so they look alike.
QtObject {
    id: columns

    // A GlobalSettings instance; widths and sort order persist there.
    required property var settings
    property real availableWidth: 0

    readonly property real rowHeight: Math.max(28, Math.round(FishUI.Units.fontMetrics.height + 12))
    readonly property real headerHeight: Math.max(26, Math.round(FishUI.Units.fontMetrics.height + 10))
    readonly property int iconSize: FishUI.Units.iconSizes.smallMedium
    readonly property real cellPadding: 10

    readonly property real minimumNameColumnWidth: 180
    readonly property real minimumColumnWidth: 64
    readonly property real defaultDateColumnWidth: 180
    readonly property real defaultSizeColumnWidth: 96
    readonly property real defaultTypeColumnWidth: 150

    property real dateColumnWidth: settings.listDateColumnWidth
    property real sizeColumnWidth: settings.listSizeColumnWidth
    property real typeColumnWidth: settings.listTypeColumnWidth

    // Narrow lists drop the least useful columns before squeezing the name.
    readonly property bool showDateColumn: availableWidth >= minimumNameColumnWidth + dateColumnWidth
    readonly property bool showSizeColumn: showDateColumn
                                           && availableWidth >= minimumNameColumnWidth + dateColumnWidth + sizeColumnWidth
    readonly property bool showTypeColumn: showSizeColumn
                                           && availableWidth >= minimumNameColumnWidth + dateColumnWidth + sizeColumnWidth + typeColumnWidth
    readonly property real nameColumnWidth: availableWidth
                                            - (showDateColumn ? dateColumnWidth : 0)
                                            - (showSizeColumn ? sizeColumnWidth : 0)
                                            - (showTypeColumn ? typeColumnWidth : 0)
    readonly property real dateColumnX: nameColumnWidth
    readonly property real sizeColumnX: dateColumnX + (showDateColumn ? dateColumnWidth : 0)
    readonly property real typeColumnX: sizeColumnX + (showSizeColumn ? sizeColumnWidth : 0)

    readonly property int sortMode: settings.sortMode
    readonly property bool sortDesc: settings.sortDesc

    function sortBy(mode) {
        if (settings.sortMode === mode) {
            settings.sortDesc = !settings.sortDesc
        } else {
            // Newest first is the useful default for dates.
            settings.sortDesc = mode === 2
            settings.sortMode = mode
        }
    }

    function saveColumnWidth(key, width) {
        settings[key] = width
    }
}
