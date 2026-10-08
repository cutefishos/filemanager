import QtQuick
import QtQuick.Controls
import FishUI 1.0 as FishUI

// The look of one file list row; views own the input handling.
Item {
    id: row

    required property QtObject columns
    // The delegate's model object: fileName, fileSize, fileType, modifiedDate,
    // originalPath, deletedDate, iconName, ...
    required property var itemModel
    property int rowIndex: 0
    property bool selected: false
    property bool dropTarget: false
    property bool dropAllowed: false
    // Adjacent selected rows read as one block, so only the outer corners round.
    property bool previousSelected: false
    property bool nextSelected: false

    property alias iconArea: _iconItem
    property alias labelArea: _label

    readonly property color selectedTextColor: {
        const color = FishUI.Theme.highlightColor
        const luminance = color.r * 0.299 + color.g * 0.587 + color.b * 0.114
        return luminance > 0.6 ? "#323238" : "white"
    }
    readonly property color primaryTextColor: selected ? selectedTextColor : FishUI.Theme.textColor
    readonly property color secondaryTextColor: selected ? selectedTextColor : FishUI.Theme.disabledTextColor
    readonly property real textOpacity: itemModel.isHidden ? 0.6 : 1.0

    Rectangle {
        id: _background
        anchors.fill: parent

        readonly property real cornerRadius: 6

        topLeftRadius: row.previousSelected ? 0 : cornerRadius
        topRightRadius: row.previousSelected ? 0 : cornerRadius
        bottomLeftRadius: row.nextSelected ? 0 : cornerRadius
        bottomRightRadius: row.nextSelected ? 0 : cornerRadius

        color: {
            if (row.dropTarget)
                return "transparent"
            if (row.selected)
                return FishUI.Theme.highlightColor
            if (row.rowIndex % 2 === 1)
                return Qt.rgba(FishUI.Theme.textColor.r, FishUI.Theme.textColor.g,
                               FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.03 : 0.025)
            return "transparent"
        }
    }

    // The drop highlight, snapped: a Rectangle border at 150% came out heavier
    // on one side, and an unsnapped fill showed a faint row outside it.
    FishUI.RoundedRectangle {
        anchors.fill: parent
        visible: row.dropTarget
        radius: _background.cornerRadius
        continuousCorners: false
        color: Qt.alpha(row.dropAllowed ? FishUI.Theme.highlightColor : "#d94b4b", 0.16)
        border.width: 2
        border.color: row.dropAllowed ? FishUI.Theme.highlightColor : "#d94b4b"
    }

    Item {
        id: _nameCell
        x: 0
        width: row.columns.nameColumnWidth
        height: parent.height

        Item {
            id: _iconItem
            x: row.columns.cellPadding
            anchors.verticalCenter: parent.verticalCenter
            width: row.columns.iconSize
            height: width
            opacity: row.itemModel.isHidden ? 0.5 : 1.0

            FishUI.IconItem {
                id: _icon
                anchors.fill: parent
                // Small sizes resolve to the flat symbolic variants in the theme.
                themeSize: 96
                source: row.itemModel.iconName
                fallback: row.itemModel.isDir ? "folder" : "unknown"
                visible: !_image.visible
            }

            Image {
                id: _image
                anchors.fill: parent
                sourceSize: Qt.size(width * 2, height * 2)
                source: row.itemModel.thumbnail ? row.itemModel.thumbnail : ""
                visible: status === Image.Ready
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                smooth: true

                layer.enabled: visible
                layer.effect: FishUI.OpacityMask {
                    maskSource: Item {
                        width: _image.width
                        height: _image.height

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.min(parent.width, _image.paintedWidth)
                            height: Math.min(parent.height, _image.paintedHeight)
                            radius: 3
                        }
                    }
                }
            }

            Image {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: -2
                anchors.bottomMargin: -2
                source: row.itemModel.isLink
                        ? "image://icontheme/emblem-symbolic-link" + FishUI.Theme.iconThemeQuery : ""
                width: 12
                height: 12
                visible: row.itemModel.isLink
                sourceSize: Qt.size(width, height)
            }
        }

        FishUI.Label {
            id: _label
            anchors.left: _iconItem.right
            anchors.leftMargin: FishUI.Units.smallSpacing + 2
            anchors.right: parent.right
            anchors.rightMargin: row.columns.cellPadding
            anchors.verticalCenter: parent.verticalCenter
            text: row.itemModel.fileName
            color: row.primaryTextColor
            opacity: row.textOpacity
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
        }
    }

    FishUI.Label {
        x: row.columns.sizeColumnX + row.columns.cellPadding
        width: row.columns.sizeColumnWidth - row.columns.cellPadding * 2
        anchors.verticalCenter: parent.verticalCenter
        visible: row.columns.showSizeColumn
        text: row.itemModel.fileSize
        color: row.secondaryTextColor
        opacity: row.textOpacity
        textFormat: Text.PlainText
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
    }

    FishUI.Label {
        x: row.columns.typeColumnX + row.columns.cellPadding
        width: row.columns.typeColumnWidth - row.columns.cellPadding * 2
        anchors.verticalCenter: parent.verticalCenter
        visible: row.columns.showTypeColumn
        text: row.itemModel.fileType
        color: row.secondaryTextColor
        opacity: row.textOpacity
        textFormat: Text.PlainText
        elide: Text.ElideRight
    }

    FishUI.Label {
        x: row.columns.originColumnX + row.columns.cellPadding
        width: row.columns.originColumnWidth - row.columns.cellPadding * 2
        anchors.verticalCenter: parent.verticalCenter
        visible: row.columns.showOriginColumn
        text: row.columns.showOriginColumn ? row.itemModel.originalPath : ""
        color: row.secondaryTextColor
        opacity: row.textOpacity
        textFormat: Text.PlainText
        elide: Text.ElideMiddle
    }

    FishUI.Label {
        x: row.columns.dateColumnX + row.columns.cellPadding
        width: row.columns.dateColumnWidth - row.columns.cellPadding * 2
        anchors.verticalCenter: parent.verticalCenter
        visible: row.columns.showDateColumn
        text: row.columns.trash ? row.itemModel.deletedDate : row.itemModel.modifiedDate
        color: row.secondaryTextColor
        opacity: row.textOpacity
        textFormat: Text.PlainText
        elide: Text.ElideRight
    }
}
