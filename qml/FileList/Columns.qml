import QtQuick
import FishUI 1.0 as FishUI

// Column layout and sort state shared by every file list, so they look alike.
QtObject {
    id: columns

    // A GlobalSettings instance; widths and sort order persist there.
    required property var settings
    property real availableWidth: 0
    // The trash shows where items came from and when they were deleted, in place
    // of the modification date and kind, and keeps a sort order of its own.
    property bool trash: false

    readonly property real rowHeight: Math.max(28, Math.round(FishUI.Units.fontMetrics.height + 12))
    readonly property real headerHeight: Math.max(26, Math.round(FishUI.Units.fontMetrics.height + 10))
    readonly property int iconSize: FishUI.Units.iconSizes.smallMedium
    readonly property real cellPadding: 10

    readonly property real minimumNameColumnWidth: 180
    readonly property real minimumColumnWidth: 64
    readonly property real defaultOriginColumnWidth: 200
    readonly property real defaultDateColumnWidth: 180
    readonly property real defaultSizeColumnWidth: 96
    readonly property real defaultTypeColumnWidth: 150

    property real originColumnWidth: settings.listOriginColumnWidth
    property real dateColumnWidth: settings.listDateColumnWidth
    property real sizeColumnWidth: settings.listSizeColumnWidth
    property real typeColumnWidth: settings.listTypeColumnWidth

    // Narrow lists drop the least useful columns before squeezing the name.
    readonly property real originRequiredWidth: minimumNameColumnWidth + (trash ? originColumnWidth : 0)
    readonly property bool showOriginColumn: trash && availableWidth >= originRequiredWidth
    readonly property bool showDateColumn: (!trash || showOriginColumn)
                                           && availableWidth >= originRequiredWidth + dateColumnWidth
    readonly property bool showSizeColumn: showDateColumn
                                           && availableWidth >= originRequiredWidth + dateColumnWidth + sizeColumnWidth
    readonly property bool showTypeColumn: !trash && showSizeColumn
                                           && availableWidth >= minimumNameColumnWidth + dateColumnWidth + sizeColumnWidth + typeColumnWidth
    readonly property real nameColumnWidth: availableWidth
                                            - (showOriginColumn ? originColumnWidth : 0)
                                            - (showDateColumn ? dateColumnWidth : 0)
                                            - (showSizeColumn ? sizeColumnWidth : 0)
                                            - (showTypeColumn ? typeColumnWidth : 0)
    readonly property real originColumnX: nameColumnWidth
    readonly property real dateColumnX: originColumnX + (showOriginColumn ? originColumnWidth : 0)
    readonly property real sizeColumnX: dateColumnX + (showDateColumn ? dateColumnWidth : 0)
    readonly property real typeColumnX: sizeColumnX + (showSizeColumn ? sizeColumnWidth : 0)

    // DirectoryModel columns.
    readonly property int nameSortMode: 0
    readonly property int sizeSortMode: 1
    readonly property int dateSortMode: trash ? 8 : 2
    readonly property int typeSortMode: 3
    readonly property int originSortMode: 7

    readonly property int sortMode: trash ? settings.trashSortMode : settings.sortMode
    readonly property bool sortDesc: trash ? settings.trashSortDesc : settings.sortDesc

    function sortBy(mode) {
        // Newest first is the useful default for dates.
        const desc = sortMode === mode ? !sortDesc : mode === dateSortMode

        if (trash) {
            settings.trashSortDesc = desc
            settings.trashSortMode = mode
        } else {
            settings.sortDesc = desc
            settings.sortMode = mode
        }
    }

    function saveColumnWidth(key, width) {
        settings[key] = width
    }
}
