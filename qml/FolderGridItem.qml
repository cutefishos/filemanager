// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtQuick.Effects

import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI

Item {
    id: control

    width: GridView.view.cellWidth
    height: GridView.view.cellHeight

    property Item iconArea: _image.visible ? _image : _icon
    property string editName: model.fileName
    property Item labelArea: _label
    property Item background: _background

    readonly property bool desktop: GridView.view.isDesktopView
    // The window grid stretches its cells to share the width; only the gap
    // between items grows, the icon and label keep the narrowest cell's width.
    readonly property real layoutWidth: desktop ? width : GridView.view.minimumCellWidth

    property int index: model.index
    property bool selected: model.selected
    property bool blank: model.blank
    property bool isDir: model.isDir
    property var fileName: model.displayName
    readonly property bool dropTarget: GridView.view.dropTargetItem === control
    readonly property bool dropAllowed: dropTarget && GridView.view.dropTargetAllowed
    readonly property color selectedTextColor: {
        const color = FishUI.Theme.highlightColor
        const luminance = color.r * 0.299 + color.g * 0.587 + color.b * 0.114
        return luminance > 0.6 ? "#323238" : "white"
    }

    // For desktop
    visible: GridView.view.isDesktopView ? !blank : true

    onSelectedChanged: updateDragImage()
    // blank and selected do not settle in the same order, so both have to ask.
    onBlankChanged: updateDragImage()

    // The drag pixmap is built from these snapshots, so one taken at the icon's
    // old cell would drag the wrong picture at the wrong offset from the cursor.
    // A resize moves every cell each frame; the snapshot waits for it to stop.
    onXChanged: dragImageTimer.restart()
    onYChanged: dragImageTimer.restart()
    onWidthChanged: dragImageTimer.restart()
    onHeightChanged: dragImageTimer.restart()
    Component.onCompleted: updateDragImage()

    Timer {
        id: dragImageTimer
        interval: 150
        onTriggered: control.updateDragImage()
    }

    // Cells are a fraction of a pixel wide; the drawing sits on whole device
    // pixels so icons and names stay sharp while the columns spread.
    // The window's ratio: Screen's is the integer wl_output scale (2 at 125%).
    readonly property real devicePixelRatio: FishUI.Dpi.ratio > 0 ? FishUI.Dpi.ratio : 1
    transform: Translate {
        x: desktop ? 0 : Math.round(control.x * control.devicePixelRatio)
                         / control.devicePixelRatio - control.x
    }

    // True when (x, y) lands on this item's icon or name rather than on the
    // empty rest of its cell; hover, presses and drops all hit test this way.
    function containsPoint(x, y) {
        var icon = iconRect()
        if (x >= icon.x && y >= icon.y
                && x <= icon.x + icon.width && y <= icon.y + icon.height)
            return true

        var text = labelRect()
        return x >= text.x && x <= text.x + text.width
                && y >= text.y && y <= text.y + text.height
    }

    // True when a rubber band over x, y, width, height (item coordinates)
    // touches the icon or name rather than only the empty rest of the cell.
    function intersectsRect(x, y, width, height) {
        var icon = iconRect()
        if (icon.x < x + width && x < icon.x + icon.width
                && icon.y < y + height && y < icon.y + icon.height)
            return true

        var text = labelRect()
        return text.x < x + width && x < text.x + text.width
                && text.y < y + height && y < text.y + text.height
    }

    // The icon as drawn, in item coordinates; a thumbnail keeps its aspect
    // ratio inside the icon box.
    function iconRect() {
        var area = control.iconArea
        var width = area === _image ? Math.min(_image.width, _image.paintedWidth) : area.width
        var height = area === _image ? Math.min(_image.height, _image.paintedHeight) : area.height

        return area.mapToItem(control, (area.width - width) / 2, (area.height - height) / 2,
                              width, height)
    }

    // The name as drawn, narrower than its box when it is short.
    function labelRect() {
        return Qt.rect(_label.x + (_label.width - _label.paintedWidth) / 2, _label.y,
                       _label.paintedWidth, _label.paintedHeight)
    }

    function updateDragImage() {
        if (!GridView.view || !selected || blank || control.width <= 0 || control.height <= 0)
            return

        // Grabbing every icon a rubber band sweeps over stalls the band; the view
        // snapshots the final selection when the band closes.
        if (GridView.view.rubberBand)
            return

        var row = GridView.view.positioner.map(control.index)
        var x = control.x
        var y = control.y
        var width = control.width
        var height = control.height

        dirModel.beginItemDragImage(row)
        var grabbing = control.grabToImage(function(result) {
            if (control.GridView.view && control.selected && !control.blank
                    && control.GridView.view.positioner.map(control.index) === row)
                dirModel.addItemDragImage(row, x, y, width, height, result.image)
            else
                dirModel.cancelItemDragImage(row)
        })
        if (!grabbing)
            dirModel.cancelItemDragImage(row)
    }

    FishUI.RoundedRectangle {
        id: _background
        width: Math.max(_iconItem.width, _label.paintedWidth)
        height: _iconItem.height + _label.paintedHeight + FishUI.Units.largeSpacing
        x: (parent.width - width) / 2
        y: _iconItem.y

        color: control.dropTarget
                ? Qt.alpha(control.dropAllowed ? FishUI.Theme.highlightColor : "#d94b4b", 0.16)
                : "transparent"
        radius: FishUI.Theme.mediumRadius
        continuousCorners: false
        border.width: control.dropTarget ? 2 : 0
        border.color: control.dropAllowed ? FishUI.Theme.highlightColor : "#d94b4b"
    }

    Item {
        id: _iconItem
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: control.desktop ? control.GridView.view.desktopIconTopMargin : FishUI.Units.smallSpacing
        anchors.bottomMargin: FishUI.Units.smallSpacing
        z: 2

        width: control.layoutWidth - FishUI.Units.largeSpacing * 2
        height: control.GridView.view.iconSize

        opacity: model.isHidden ? 0.5 : 1.0

        // Snapped to the device pixel grid wherever the cell and view land.
        FishUI.IconItem {
            id: _icon
            width: control.GridView.view.iconSize
            height: control.GridView.view.iconSize
            anchors.centerIn: parent
            source: model.iconName
            fallback: model.isDir ? "folder" : "unknown"
            visible: !_image.visible
        }

        Image {
            id: _image
            anchors.fill: parent
            anchors.topMargin: FishUI.Units.smallSpacing
            anchors.leftMargin: FishUI.Units.smallSpacing
            anchors.rightMargin: FishUI.Units.smallSpacing
            fillMode: Image.PreserveAspectFit
            visible: status === Image.Ready
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            sourceSize: Qt.size(width, height)
            source: model.thumbnail ? model.thumbnail : ""
            asynchronous: true
            cache: false
            smooth: true
            antialiasing: true

            layer.enabled: _image.visible
            layer.effect: FishUI.OpacityMask {
                maskSource: Item {
                    width: _image.width
                    height: _image.height

                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, _image.paintedWidth)
                        height: Math.min(parent.height, _image.paintedHeight)
                        radius: height * 0.1
                    }
                }
            }
        }

        // A faint light rim keeps a thumbnail's edge readable on a wallpaper
        // of the same tone.
        FishUI.RoundedRectangle {
            anchors.centerIn: _image
            width: Math.min(_image.width, _image.paintedWidth)
            height: Math.min(_image.height, _image.paintedHeight)
            radius: height * 0.1
            visible: control.desktop && _image.visible
            continuousCorners: false
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.45)
        }

        Image {
            anchors.right: _icon.visible ? _icon.right : _image.right
            anchors.bottom: _icon.visible ? _icon.bottom : _image.bottom
            source: "image://icontheme/emblem-symbolic-link" + FishUI.Theme.iconThemeQuery
            width: control.desktop ? Math.max(16, Math.round(control.GridView.view.iconSize / 4)) : 16
            height: width
            visible: model.isLink
            sourceSize: Qt.size(width, height)
        }
    }

    // Keep these outside _iconItem: an effect nested in its own source holds a
    // window ref that is never dropped, which crashes once the window is gone.
    //
    // A blurred black silhouette rather than a shadow on the icon itself, so
    // the selection tint below colours only the icon.
    //
    // The effects are loaded only while shown: instantiating them in every
    // delegate made up over a third of the cost of scrolling a large folder.
    Loader {
        z: 1
        x: _iconItem.x
        y: _iconItem.y + 1
        width: _iconItem.width
        height: _iconItem.height
        active: control.desktop

        sourceComponent: MultiEffect {
            source: _iconItem
            opacity: 0.15 * _iconItem.opacity
            brightness: -1.0
            blurEnabled: true
            blurMax: 16
            blur: 0.5
        }
    }

    Loader {
        anchors.fill: _iconItem
        z: 2
        active: control.selected

        sourceComponent: FishUI.ColorOverlay {
            source: _iconItem
            color: FishUI.Theme.highlightColor
            opacity: 0.5 * _iconItem.opacity
        }
    }

    FishUI.Label {
        id: _label
        property var selectionLines: []

        z: 2
        anchors.top: _iconItem.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: control.desktop ? control.GridView.view.desktopLabelSpacing : FishUI.Units.smallSpacing
        maximumLineCount: control.desktop ? 2 : (control.selected ? 3 : 2)
        font: control.desktop ? control.GridView.view.desktopLabelFont : FishUI.Theme.defaultFont
        horizontalAlignment: Text.AlignHCenter
        width: control.desktop ? control.layoutWidth - control.GridView.view.desktopLabelInset * 2
                               : control.layoutWidth - FishUI.Units.largeSpacing * 2 - FishUI.Units.smallSpacing
        textFormat: Text.PlainText
        elide: Qt.ElideRight
        wrapMode: Text.Wrap
        text: control.desktop ? control.GridView.view.desktopLabelText(control.fileName, width)
                              : control.fileName
        color: selected ? control.selectedTextColor
                        : control.GridView.view.isDesktopView ? "white" : FishUI.Theme.textColor
        opacity: model.isHidden ? 0.8 : 1.0
        // A layer effect on the label itself would lose hinting and fringe the
        // glyphs; the faint 1px edge hides the subpixel fringes instead.
        style: control.desktop && !control.selected ? Text.Raised : Text.Normal
        styleColor: Qt.rgba(0, 0, 0, 0.2)

        onLineLaidOut: function(line) {
            var lines = selectionLines.slice(0)
            var naturalWidth = isFinite(line.implicitWidth) ? line.implicitWidth : line.width
            var lineWidth = Math.max(0, Math.min(width, naturalWidth))

            lines[line.number] = Qt.rect((width - lineWidth) / 2, line.y,
                                         lineWidth, line.height)
            if (line.isLast)
                lines.length = line.number + 1
            selectionLines = lines
        }
    }

    Canvas {
        id: _selectionBackground

        readonly property real hPadding: control.desktop ? 4 : FishUI.Units.smallSpacing / 2
        readonly property real vPadding: control.desktop ? 1 : 0.5
        readonly property color fillColor: FishUI.Theme.highlightColor
        property var lineGeometry: _label.selectionLines

        z: 1
        x: _label.x - hPadding
        y: _label.y - vPadding
        visible: control.selected && _label.visible
        width: _label.width + hPadding * 2
        height: _label.height + vPadding * 2
        antialiasing: true

        onLineGeometryChanged: requestPaint()
        onFillColorChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onVisibleChanged: requestPaint()

        function roundedRect(context, x, y, width, height, radius) {
            var r = Math.min(radius, width / 2, height / 2)

            context.moveTo(x + r, y)
            context.lineTo(x + width - r, y)
            context.quadraticCurveTo(x + width, y, x + width, y + r)
            context.lineTo(x + width, y + height - r)
            context.quadraticCurveTo(x + width, y + height,
                                     x + width - r, y + height)
            context.lineTo(x + r, y + height)
            context.quadraticCurveTo(x, y + height, x, y + height - r)
            context.lineTo(x, y + r)
            context.quadraticCurveTo(x, y, x + r, y)
            context.closePath()
        }

        onPaint: {
            var context = getContext("2d")
            context.clearRect(0, 0, width, height)
            context.fillStyle = fillColor
            context.beginPath()

            for (var i = 0; i < lineGeometry.length; ++i) {
                var line = lineGeometry[i]
                if (!line || line.width <= 0 || line.height <= 0)
                    continue

                roundedRect(context, line.x, line.y,
                            line.width + hPadding * 2,
                            line.height + vPadding * 2, 4)

                if (i > 0) {
                    var previous = lineGeometry[i - 1]
                    if (!previous)
                        continue
                    var bridgeLeft = Math.max(previous.x, line.x)
                    var bridgeRight = Math.min(previous.x + previous.width,
                                               line.x + line.width) + hPadding * 2
                    var boundary = line.y + vPadding

                    if (bridgeRight > bridgeLeft)
                        context.rect(bridgeLeft, boundary - 2,
                                     bridgeRight - bridgeLeft, 4)
                }
            }

            context.fill()
        }
    }

    // Soft halo from a blurred black copy; only its alpha survives the blur.
    Item {
        z: 1
        x: _label.x - 8
        y: _label.y - 8 + 1
        width: _label.width + 16
        height: _label.height + 16
        visible: control.desktop && _label.visible && !control.selected
        opacity: model.isHidden ? 0.35 : 0.65

        Text {
            x: 8
            y: 8
            width: _label.width
            text: _label.text
            font: _label.font
            textFormat: _label.textFormat
            horizontalAlignment: _label.horizontalAlignment
            wrapMode: _label.wrapMode
            maximumLineCount: _label.maximumLineCount
            elide: _label.elide
            color: "black"
        }

        layer.enabled: visible
        layer.effect: FishUI.FastBlur {
            radius: 6
        }
    }
}
