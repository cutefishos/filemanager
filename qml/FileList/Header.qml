import QtQuick
import QtQuick.Controls
import FishUI 1.0 as FishUI

// Blocks input over itself because rows scroll beneath it.
MouseArea {
    id: header

    required property QtObject columns
    // Where the rows start and end inside the header's width.
    property real leftInset: 0
    property real rightInset: 0
    property color backgroundColor: FishUI.Theme.secondBackgroundColor

    height: columns.headerHeight
    acceptedButtons: Qt.AllButtons
    hoverEnabled: true
    onWheel: function(wheel) { wheel.accepted = false }

    component HeaderCell: MouseArea {
        id: _cell

        property QtObject columns
        property string title
        property int sortMode: 0
        property int alignment: Text.AlignLeft
        readonly property bool sorted: columns.sortMode === sortMode

        height: parent ? parent.height : 0
        acceptedButtons: Qt.LeftButton
        onClicked: columns.sortBy(sortMode)

        Accessible.role: Accessible.ColumnHeader
        Accessible.name: title

        Row {
            id: _titleRow
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter
            x: _cell.alignment === Text.AlignRight
               ? Math.max(_cell.columns.cellPadding, _cell.width - _cell.columns.cellPadding - width)
               : _cell.columns.cellPadding
            width: Math.min(implicitWidth, _cell.width - _cell.columns.cellPadding * 2)

            Label {
                id: _titleLabel
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, _cell.width - _cell.columns.cellPadding * 2
                                - (_chevron.visible ? _chevron.width + _titleRow.spacing : 0))
                text: _cell.title
                elide: Text.ElideRight
                font.pointSize: Math.max(1, FishUI.Theme.defaultFont.pointSize * 0.92)
                font.weight: _cell.sorted ? Font.DemiBold : Font.Normal
                color: _cell.sorted ? FishUI.Theme.textColor : FishUI.Theme.disabledTextColor
            }

            FishUI.Icon {
                id: _chevron
                anchors.verticalCenter: parent.verticalCenter
                width: 12
                height: 12
                visible: _cell.sorted
                name: _cell.columns.sortDesc ? "chevron-down" : "chevron-up"
                color: _titleLabel.color
            }
        }
    }

    component ColumnResizer: MouseArea {
        id: _resizer

        property QtObject columns
        // The column whose leading edge this handle drags.
        property string widthProperty
        property string settingsProperty
        property real defaultWidth
        property real startWidth: 0
        property real startX: 0

        width: 9
        height: parent ? parent.height : 0
        z: 1
        cursorShape: Qt.SplitHCursor
        hoverEnabled: true
        preventStealing: true

        onPressed: function(mouse) {
            startWidth = columns[widthProperty]
            startX = mapToItem(null, mouse.x, 0).x
        }

        onPositionChanged: function(mouse) {
            if (!pressed)
                return

            var delta = startX - mapToItem(null, mouse.x, 0).x
            var maximum = Math.floor(columns[widthProperty] + columns.nameColumnWidth - columns.minimumNameColumnWidth)
            columns[widthProperty] = Math.max(columns.minimumColumnWidth,
                                              Math.min(maximum, Math.round(startWidth + delta)))
        }

        onReleased: columns.saveColumnWidth(settingsProperty, columns[widthProperty])
        onDoubleClicked: {
            columns[widthProperty] = defaultWidth
            columns.saveColumnWidth(settingsProperty, defaultWidth)
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: parent.height * 0.5
            color: FishUI.Theme.textColor
            opacity: _resizer.containsMouse || _resizer.pressed ? 0.3 : 0.12
        }
    }

    Rectangle {
        anchors.fill: parent
        color: header.backgroundColor
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: header.leftInset
        anchors.rightMargin: header.rightInset
        height: 1
        color: FishUI.Theme.textColor
        opacity: FishUI.Theme.darkMode ? 0.12 : 0.08
    }

    Item {
        x: header.leftInset
        width: header.columns.availableWidth
        height: parent.height

        HeaderCell {
            columns: header.columns
            x: 0
            width: header.columns.nameColumnWidth
            title: qsTr("Name")
            sortMode: 0
        }

        HeaderCell {
            columns: header.columns
            x: header.columns.dateColumnX
            width: header.columns.dateColumnWidth
            visible: header.columns.showDateColumn
            title: qsTr("Date Modified")
            sortMode: 2
        }

        HeaderCell {
            columns: header.columns
            x: header.columns.sizeColumnX
            width: header.columns.sizeColumnWidth
            visible: header.columns.showSizeColumn
            title: qsTr("Size")
            sortMode: 1
            alignment: Text.AlignRight
        }

        HeaderCell {
            columns: header.columns
            x: header.columns.typeColumnX
            width: header.columns.typeColumnWidth
            visible: header.columns.showTypeColumn
            title: qsTr("Kind")
            sortMode: 3
        }

        ColumnResizer {
            columns: header.columns
            x: header.columns.dateColumnX - width / 2
            visible: header.columns.showDateColumn
            widthProperty: "dateColumnWidth"
            settingsProperty: "listDateColumnWidth"
            defaultWidth: header.columns.defaultDateColumnWidth
        }

        ColumnResizer {
            columns: header.columns
            x: header.columns.sizeColumnX - width / 2
            visible: header.columns.showSizeColumn
            widthProperty: "sizeColumnWidth"
            settingsProperty: "listSizeColumnWidth"
            defaultWidth: header.columns.defaultSizeColumnWidth
        }

        ColumnResizer {
            columns: header.columns
            x: header.columns.typeColumnX - width / 2
            visible: header.columns.showTypeColumn
            widthProperty: "typeColumnWidth"
            settingsProperty: "listTypeColumnWidth"
            defaultWidth: header.columns.defaultTypeColumnWidth
        }
    }
}
