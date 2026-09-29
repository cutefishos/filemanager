/*
 * Copyright (C) 2021 revenmartin <revenmartin@gmail.com>
 * Copyright (C) 2014-2015 by Eike Hein <hein@kde.org>
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

import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Cutefish.FileManager 1.0
import Cutefish.DragDrop 1.0 as DragDrop
import FishUI 1.0 as FishUI

GridView {
    id: control

    objectName: "FolderGridView"

    property bool isDesktopView: false
    // Bars floating over the top of the view; the content scrolls beneath them.
    property real topInset: 0
    readonly property real headerHeight: topInset
    property bool useCustomContextMenu: false
    // Desktop input extends beyond the cells reserved for icons.
    property Item interactionSurface: control
    property bool pressActive: false

    property Item rubberBand: null
    property Item hoveredItem: null
    property Item pressedItem: null
    property Item dropTargetItem: null
    property bool dropTargetAllowed: false

    property alias positions: positioner.positions
    property alias positioner: positioner

    // The window grid counts its columns from the width alone and shares the
    // width between them. Counting from cellWidth, a binding on the width too,
    // saw the old cell with the new width and dropped a column for an instant
    // on every pixel of a resize.
    readonly property int gridColumns: isDesktopView
            ? Math.max(1, Math.floor((width - leftMargin - rightMargin) / cellWidth))
            : Math.max(1, Math.floor((width - leftMargin - rightMargin) / minimumCellWidth))
    readonly property real minimumCellWidth: iconSize + FishUI.Units.largeSpacing * 4
    readonly property int gridRows: Math.max(1, Math.floor((height - topMargin - bottomMargin) / cellHeight))
    readonly property bool columnFlow: control.flow === GridView.FlowTopToBottom

    // Where inside its cell the icon was grabbed, so a drop lands the icon under
    // the cursor rather than offset by the grab point.
    property int dragAnchorIndex: -1
    property point dragGrabOffset: Qt.point(0, 0)

    property int verticalDropHitscanOffset: 0
    property real dragPositionY: -1
    property int dragScrollDirection: 0
    property int dragScrollElapsed: 0
    property real dragScrollVelocity: 0
    property bool dragScrollActive: false

    readonly property real dragScrollEdge: 48
    readonly property real rubberBandScrollEdge: 24
    readonly property int dragScrollRamp: 180
    readonly property real dragScrollMaxSpeed: 560

    property real pressX: -1
    property real pressY: -1

    property int dragX: -1
    property int dragY: -1

    property bool ctrlPressed: false
    property bool shiftPressed: false

    property variant cPress: null
    property real lastMouseX: -1
    property real lastMouseY: -1
    property Item editor: null
    property int anchorIndex: 0

    property var iconSize: settings.gridIconSize
    property var maximumIconSize: settings.maximumIconSize
    property var minimumIconSize: 22 //settings.minimumIconSize

    property variant cachedRectangleSelection: null
    property string cachedRectangleSelectionMode: ""

    property bool scrollLeft: false
    property bool scrollRight: false
    property bool scrollUp: false
    property bool scrollDown: false

    signal keyPress(var event)
    signal contextMenuRequested()

    cacheBuffer: Math.max(0, control.height * 1.5)
    reuseItems: true

    onIconSizeChanged: {
        // Stale positions misplace the selection after a resize; the desktop keeps
        // its own positions, which a reset would throw away.
        if (!positioner.enabled)
            positioner.reset()
    }

    onCountChanged: {
        if (!positioner.enabled)
            positioner.reset()
    }

    property int laidColumns: 0
    property real laidCellHeight: 0

    onGridColumnsChanged: reflow()
    onCellHeightChanged: reflow()
    Component.onCompleted: {
        laidColumns = gridColumns
        laidCellHeight = cellHeight
    }

    // A new column count or icon size moves every row; the one at the top of
    // the view stays there instead of whatever lands under it.
    function reflow() {
        var fromColumns = laidColumns
        var fromRowHeight = laidCellHeight
        laidColumns = gridColumns
        laidCellHeight = cellHeight
        if (isDesktopView || fromColumns <= 0 || fromRowHeight <= 0
                || (fromColumns === gridColumns && fromRowHeight === cellHeight))
            return

        var top = contentY + headerHeight
        if (top <= 0)
            return
        var row = Math.floor(top / fromRowHeight)
        var within = (top - row * fromRowHeight) * cellHeight / fromRowHeight
        contentY = Math.floor(row * fromColumns / gridColumns) * cellHeight + within - headerHeight
    }

    // GridView scrolls the current item into view but knows nothing of the
    // bars over its top, so an item it scrolls up to would end up beneath them.
    onCurrentIndexChanged: Qt.callLater(revealCurrentItem)

    function revealCurrentItem() {
        if (currentItem && currentItem.y < contentY + headerHeight)
            contentY = currentItem.y - headerHeight
    }

    function effectiveNavDirection(flow, layoutDirection, direction) {
        if (direction === Qt.LeftArrow) {
            if (flow === GridView.FlowLeftToRight) {
                if (layoutDirection === Qt.LeftToRight) {
                    return Qt.LeftArrow;
                } else {
                    return Qt.RightArrow;
                }
            } else {
                if (layoutDirection === Qt.LeftToRight) {
                    return Qt.UpArrow;
                } else {
                    return Qt.DownArrow;
                }
            }
        } else if (direction === Qt.RightArrow) {
            if (flow === GridView.FlowLeftToRight) {
                if (layoutDirection === Qt.LeftToRight) {
                    return Qt.RightArrow;
                } else {
                    return Qt.LeftArrow;
                }
            } else {
                if (layoutDirection === Qt.LeftToRight) {
                    return Qt.DownArrow;
                } else {
                    return Qt.UpArrow;
                }
            }
        } else if (direction === Qt.UpArrow) {
            if (flow === GridView.FlowLeftToRight) {
                return Qt.UpArrow;
            } else {
                return Qt.LeftArrow;
            }
        } else if (direction === Qt.DownArrow) {
            if (flow === GridView.FlowLeftToRight) {
                return Qt.DownArrow;
            } else {
                return Qt.RightArrow
            }
        }
    }

    function rename() {
        if (!control.currentItem || control.editor)
            return

        var action = dirModel.action("rename")
        if (action && !action.enabled)
            return

        control.editor = editorComponent.createObject(control, {
            "targetItem": control.currentItem,
            "originalName": control.currentItem.editName,
            "selectionEnd": dirModel.fileExtensionBoundary(positioner.map(control.currentIndex))
        })
        control.editor.begin()
    }

    function cancelRename() {
        if (control.editor)
            control.editor.cancel()
    }

    // x, y are the click that opened the menu, in view coordinates.
    function openContextMenu(modifiers, x, y) {
        // A new folder or file belongs on the cell that was right-clicked.
        if (positioner.enabled) {
            var pos = mapToItem(control.contentItem, x, y)
            var cell = cellAt(pos.x, pos.y)
            dirModel.setNewDocumentPosition(x, y, cellIndex(cell.x, cell.y))
        }

        if (control.useCustomContextMenu) {
            dirModel.prepareContextMenu()
            control.contextMenuRequested()
        } else {
            dirModel.openContextMenu(null, modifiers)
        }
    }

    function reset() {
        pressActive = false
        currentIndex = -1
        anchorIndex = 0
        cancelRename()
        hoveredItem = null
        pressedItem = null
        cPress = null
    }

    function drop(target, event, pos) {
        var dropPos = mapToItem(control.contentItem, pos.x, pos.y)
        var dropIndex = control.indexAt(dropPos.x, dropPos.y)
        var dragPos = mapToItem(control.contentItem, control.dragX, control.dragY)
        var dragIndex = control.indexAt(dragPos.x, dragPos.y)
        var row = dropItemAt(dropPos)

        if ((control.dragX !== -1 && dragIndex === dropIndex)
                || !dirModel.canDrop(event.mimeData, row, event.dropAction)) {
            event.ignore()
            control.clearDropTarget()
            return
        }

        event.accept(event.dropAction)

        // Files dropped between icons land on the cell under the cursor.
        if (positioner.enabled && row === -1) {
            var cell = cellAt(dropPos.x, dropPos.y)
            dirModel.setNewDocumentPosition(pos.x, pos.y, cellIndex(cell.x, cell.y))
        }

        dirModel.drop(control, event, row)
        control.clearDropTarget()
    }

    // The row of the folder a drop at pos (content coordinates) goes into, or
    // -1 for the folder shown. Only a folder's icon and name take the drop, so
    // the rest of every cell still reaches the folder shown when icons fill it.
    function dropItemAt(pos) {
        var item = control.itemAt(pos.x, pos.y)

        if (!item || item.blank || !item.isDir)
            return -1

        var local = control.contentItem.mapToItem(item, pos.x, pos.y)
        return item.containsPoint(local.x, local.y) ? positioner.map(item.index) : -1
    }

    function updateDropTarget(event, pos) {
        if (pos.x < 0 || pos.y < 0 || pos.x > control.width || pos.y > control.height) {
            control.clearDropTarget()
            return false
        }

        var dropPos = mapToItem(control.contentItem, pos.x, pos.y)
        var item = control.itemAt(dropPos.x, dropPos.y)
        var row = control.dropItemAt(dropPos)
        var isDraggedItem = dirModel.dragging && item && item.selected
        control.dropTargetItem = row >= 0 && item && !item.blank && item.isDir && !isDraggedItem
                ? item : null
        control.dropTargetAllowed = dirModel.canDrop(event.mimeData, row,
                                                     control.dropActionForEvent(event))
        return control.dropTargetAllowed
    }

    function dropActionForEvent(event) {
        if (event.modifiers === Qt.NoModifier && event.possibleActions & Qt.MoveAction)
            return Qt.MoveAction
        if (event.proposedAction !== Qt.IgnoreAction)
            return event.proposedAction
        if (event.possibleActions & Qt.MoveAction)
            return Qt.MoveAction
        if (event.possibleActions & Qt.CopyAction)
            return Qt.CopyAction
        return Qt.IgnoreAction
    }

    function clearDropTarget() {
        control.dropTargetItem = null
        control.dropTargetAllowed = false
    }

    function updateDragScrollPosition(y) {
        dragPositionY = y
        dragScrollActive = true

        var edge = control.rubberBand ? rubberBandScrollEdge : dragScrollEdge
        var direction = y < headerHeight + edge ? -1
                        : y > control.height - edge ? 1 : 0
        if (direction === dragScrollDirection)
            return

        dragScrollDirection = direction
        dragScrollElapsed = 0
        dragScrollVelocity = 0
    }

    function stopDragScroll() {
        dragPositionY = -1
        dragScrollDirection = 0
        dragScrollElapsed = 0
        dragScrollVelocity = 0
        dragScrollActive = false
    }

    // frameTime is in seconds; a stalled frame must not jump the view.
    function autoScrollDuringDrag(frameTime) {
        if (!dragScrollActive || dragScrollDirection === 0 || control.isDesktopView)
            return

        var interval = Math.min(50, frameTime * 1000)
        dragScrollElapsed += interval
        var minimum = control.originY - control.topMargin
        var maximum = Math.max(minimum, control.contentHeight - control.height + control.originY)
        var edge = control.rubberBand ? rubberBandScrollEdge : dragScrollEdge
        var distance = dragScrollDirection < 0
                ? headerHeight + edge - dragPositionY
                : dragPositionY - (control.height - edge)
        var edgeProgress = Math.max(0, Math.min(1, distance / edge))
        var rampProgress = Math.max(0, Math.min(1,
                                                  dragScrollElapsed / dragScrollRamp))
        var ramp = 1 - Math.pow(1 - rampProgress, 3)
        var targetVelocity = dragScrollMaxSpeed * Math.pow(edgeProgress, 1.5) * ramp

        // Eases a quarter of the way per 16 ms frame at any refresh rate.
        dragScrollVelocity += (targetVelocity - dragScrollVelocity) * (1 - Math.pow(0.75, interval / 16))
        var next = control.contentY + dragScrollDirection * dragScrollVelocity * interval / 1000

        control.contentY = Math.max(minimum, Math.min(maximum, next))
    }

    function updateRubberBand(mouseX, mouseY) {
        var rB = control.rubberBand
        if (!rB || !cPress)
            return

        var cPos = control.mapToItem(control.contentItem, mouseX, mouseY)
        var bounds = control.interactionSurface.mapToItem(control.contentItem, 0, 0,
                control.interactionSurface.width, control.interactionSurface.height)
        var leftEdge = control.isDesktopView ? bounds.x : Math.min(control.contentX, control.originX)

        if (cPos.x < cPress.x) {
            rB.x = Math.max(leftEdge, cPos.x)
            rB.width = Math.abs(rB.x - cPress.x)
        } else {
            rB.x = cPress.x
            const ceil = control.isDesktopView ? bounds.x + bounds.width
                                              : Math.max(control.width, control.contentItem.width) + leftEdge
            rB.width = Math.min(ceil - rB.x, Math.abs(rB.x - cPos.x))
        }

        if (cPos.y < cPress.y) {
            rB.y = Math.max(control.isDesktopView ? bounds.y : control.originY, cPos.y)
            rB.height = Math.abs(rB.y - cPress.y)
        } else {
            rB.y = cPress.y
            const ceil = control.isDesktopView ? bounds.y + bounds.height
                                              : Math.max(control.originY + control.contentHeight,
                                                         control.contentY + control.height)
            rB.height = Math.min(ceil - rB.y, Math.abs(rB.y - cPos.y))
        }

        // Ensure rubberband is at least 1px in size or else it will become
        // invisible and not match any items.
        rB.width = Math.max(1, rB.width)
        rB.height = Math.max(1, rB.height)

        control.rectangleSelect(rB.x, rB.y, rB.width, rB.height)
    }

    highlightMoveDuration: 0
    keyNavigationEnabled : true
    keyNavigationWraps : true
    Keys.onPressed: function(event) {
        control.keyPress(event)

        if (event.key === Qt.Key_Control) {
            ctrlPressed = true
        } else if (event.key === Qt.Key_Shift) {
            shiftPressed = true

            if (currentIndex != -1)
                anchorIndex = currentIndex
        } else if ((event.key === Qt.Key_Equal || event.key === Qt.Key_Plus) && event.modifiers & Qt.ControlModifier) {
            control.increaseIconSize()
        } else if (event.key === Qt.Key_Minus && event.modifiers & Qt.ControlModifier) {
            control.decreaseIconSize()
        }
    }
    Keys.onReleased: function(event) {
        if (event.key === Qt.Key_Control) {
            ctrlPressed = false
        } else if (event.key === Qt.Key_Shift) {
            shiftPressed = false
            anchorIndex = 0
        }
    }
    Keys.onEscapePressed: function(event) {
        if (!editor || !editor.targetItem) {
            dirModel.clearSelection()
            event.accepted = false
        }
    }
    Keys.onUpPressed: function(event) {
        if (!editor || !editor.targetItem) {
            var newIndex = positioner.nearestItem(currentIndex,
                                                  effectiveNavDirection(control.flow, control.effectiveLayoutDirection, Qt.UpArrow))
            if (newIndex !== -1) {
                currentIndex = newIndex
                updateSelection(event.modifiers)
            }
        }
    }
    Keys.onDownPressed: function(event) {
        if (!editor || !editor.targetItem) {
            var newIndex = positioner.nearestItem(currentIndex,
                                                  effectiveNavDirection(control.flow, control.effectiveLayoutDirection, Qt.DownArrow))
            if (newIndex !== -1) {
                currentIndex = newIndex
                updateSelection(event.modifiers)
            }
        }
    }
    Keys.onLeftPressed: function(event) {
        if (!editor || !editor.targetItem) {
            var newIndex = positioner.nearestItem(currentIndex,
                                                  effectiveNavDirection(control.flow, control.effectiveLayoutDirection, Qt.LeftArrow))
            if (newIndex !== -1) {
                currentIndex = newIndex
                updateSelection(event.modifiers)
            }
        }
    }
    Keys.onRightPressed: function(event) {
        if (!editor || !editor.targetItem) {
            var newIndex = positioner.nearestItem(currentIndex,
                                                  effectiveNavDirection(control.flow, control.effectiveLayoutDirection, Qt.RightArrow))
            if (newIndex !== -1) {
                currentIndex = newIndex
                updateSelection(event.modifiers)
            }
        }
    }

    // Desktop cell metrics, shared with FolderGridItem. Text steps up once per
    // 32px of icon so labels stay at roughly a fifth of the icon, as on macOS.
    readonly property int desktopLabelPixelSize: Math.min(14, 12 + Math.floor(Math.max(0, iconSize - 64) / 32))
    readonly property font desktopLabelFont: Qt.font({ family: FishUI.Theme.fontFamily,
                                                       pixelSize: desktopLabelPixelSize,
                                                       weight: Font.DemiBold })
    readonly property int desktopIconTopMargin: 6
    readonly property int desktopLabelSpacing: 5
    readonly property int desktopLabelInset: 6
    readonly property int desktopCellBottomMargin: 8
    readonly property real desktopLabelHeight: desktopLabelMetrics.height

    FontMetrics {
        id: desktopLabelMetrics
        font: control.desktopLabelFont
    }

    // Lays a name out on at most two lines and elides the middle of the second,
    // so a long name keeps both how it starts and its extension. Text itself
    // can only elide the end of a wrapped paragraph.
    function desktopLabelText(name, width) {
        // Method calls are not tracked; reading the font makes a binding that
        // calls this lay the name out again when the label size steps.
        void desktopLabelMetrics.font

        // Measured advances can round below what the label lays out.
        var room = width - 2

        if (room <= 0 || desktopLabelMetrics.advanceWidth(name) <= room)
            return name

        var fit = 0
        var low = 1
        var high = name.length

        while (low <= high) {
            var middle = Math.floor((low + high) / 2)

            if (desktopLabelMetrics.advanceWidth(name.substring(0, middle)) <= room) {
                fit = middle
                low = middle + 1
            } else {
                high = middle - 1
            }
        }

        if (fit === 0)
            return desktopLabelMetrics.elidedText(name, Qt.ElideMiddle, room)

        // Break after a word, as wrapping would, unless a single word fills the line.
        var breakAt = fit
        if (name[fit] !== " ") {
            for (var i = fit; i > 0; --i) {
                if (name[i - 1] === " " || name[i - 1] === "-" || name[i - 1] === "_") {
                    breakAt = i
                    break
                }
            }
        }

        var first = name.substring(0, breakAt).replace(/\s+$/, "")
        var rest = name.substring(breakAt).replace(/^\s+/, "")

        if (desktopLabelMetrics.advanceWidth(rest) > room)
            rest = desktopLabelMetrics.elidedText(rest, Qt.ElideMiddle, room)

        return first + "\n" + rest
    }

    cellHeight: {
        var iconHeight = iconSize + (FishUI.Units.fontMetrics.height * 2) + FishUI.Units.largeSpacing * 2
        if (isDesktopView) {
            return Math.ceil(desktopIconTopMargin + iconSize + desktopLabelSpacing
                             + desktopLabelHeight * 2 + desktopCellBottomMargin)
        }
        return iconHeight
    }

    cellWidth: {
        if (isDesktopView)
            return Math.ceil(Math.max(88, iconSize + 44))
        // The columns share the width evenly and follow it smoothly; each item
        // snaps its own drawing to the pixel grid. Whole-pixel cells moved the
        // last column several pixels at once every few pixels of resize.
        // Rounded down a 64th of a pixel: GridView counts the columns itself,
        // and a cell a hair wider than the share would lose one.
        var available = control.width - leftMargin - rightMargin
        return Math.max(minimumCellWidth, Math.floor(available / gridColumns * 64) / 64)
    }

    clip: !isDesktopView
    // Dragging the view must rubber band select, not flick the content around
    // like a touch interface. WheelHandler and the scroll bar do the scrolling.
    interactive: false
    currentIndex: -1
    FishUI.ScrollBar.vertical: FishUI.ScrollBar {
        topPadding: control.headerHeight
    }

    FishUI.WheelHandler {
        target: control
    }

    onPressXChanged: {
        cPress = mapToItem(control.contentItem, pressX, pressY)
    }

    onPressYChanged: {
        cPress = mapToItem(control.contentItem, pressX, pressY)
    }

    // The band's far corner stays under the pointer while the view scrolls
    // beneath it, whether by autoscroll or the wheel.
    onContentXChanged: {
        cancelRename()

        if (rubberBand)
            updateRubberBand(lastMouseX, lastMouseY)
    }

    onContentYChanged: {
        cancelRename()

        if (rubberBand)
            updateRubberBand(lastMouseX, lastMouseY)
    }

    // Ctrl toggles what the band covers against the selection it started
    // from, Shift adds to it.
    function applyRectangleSelection(indices) {
        var mode = control.ctrlPressed ? "toggle" : control.shiftPressed ? "extend" : "replace"
        var previous = control.cachedRectangleSelection

        // Pointer moves far outnumber changes to what the band covers.
        if (previous && mode === control.cachedRectangleSelectionMode
                && previous.length === indices.length
                && previous.every(function(index, i) { return index === indices[i] }))
            return

        control.cachedRectangleSelection = indices
        control.cachedRectangleSelectionMode = mode

        dirModel.updateSelection(indices.map(function(index) {
            return positioner.map(index)
        }), mode === "toggle", mode === "extend")
    }

    Positioner {
        id: positioner
        // Only the desktop keeps icons in fixed cells; in the window the view
        // stays a plain, always-packed grid.
        enabled: control.isDesktopView
        folderModel: dirModel
        perStripe: control.columnFlow ? control.gridRows : control.gridColumns
        stripes: control.columnFlow ? control.gridColumns : control.gridRows
    }

    // Cell containing a point in content coordinates.
    function cellAt(cx, cy) {
        return clampCell(Math.floor(cx / control.cellWidth), Math.floor(cy / control.cellHeight))
    }

    // Cell an icon whose top left corner is at cx, cy snaps to: the nearest one,
    // so half a cell of travel is enough to move it, the way a drag should feel.
    function cellNear(cx, cy) {
        return clampCell(Math.round(cx / control.cellWidth), Math.round(cy / control.cellHeight))
    }

    function clampCell(col, row) {
        col = Math.max(0, Math.min(control.gridColumns - 1, col))
        row = Math.max(0, Math.min(control.gridRows - 1, row))

        if (control.effectiveLayoutDirection === Qt.RightToLeft)
            col = control.gridColumns - 1 - col

        return Qt.point(col, row)
    }

    function cellIndex(col, row) {
        return control.columnFlow ? (col * control.gridRows) + row
                                  : (row * control.gridColumns) + col
    }

    function cellOfIndex(index) {
        return control.columnFlow ? Qt.point(Math.floor(index / control.gridRows), index % control.gridRows)
                                  : Qt.point(index % control.gridColumns, Math.floor(index / control.gridColumns))
    }

    // A drop inside the view: pin the dropped icons to the cell under the cursor,
    // keeping the relative arrangement of a multiple selection. anchorIndex is
    // the cell the drag was started from, or -1 for a drop that comes from
    // somewhere else -- those land on the cell under the cursor.
    function moveToCell(x, y, urls, anchorIndex, grabOffset) {
        if (!positioner.enabled)
            return

        // A drag that started on one of the icons keeps the selection's shape:
        // every icon moves by the same number of cells as the grabbed one.
        var anchor = anchorIndex >= 0 ? cellOfIndex(anchorIndex) : null
        var grab = anchor ? grabOffset : Qt.point(0, 0)
        var pos = mapToItem(control.contentItem, x, y)
        // With an anchor the drag carries the icon itself, so snap the icon; a
        // drop from elsewhere only has a cursor, which lands on the cell it is over.
        var target = anchor ? cellNear(pos.x - grab.x, pos.y - grab.y) : cellAt(pos.x, pos.y)
        var moves = []

        for (var i = 0; i < urls.length; ++i) {
            var from = positioner.indexForUrl(urls[i])

            if (from === -1)
                continue

            var cell = cellOfIndex(from)
            var col = anchor ? target.x + (cell.x - anchor.x) : target.x
            var row = anchor ? target.y + (cell.y - anchor.y) : target.y

            col = Math.max(0, Math.min(control.gridColumns - 1, col))
            row = Math.max(0, Math.min(control.gridRows - 1, row))

            moves.push(from)
            moves.push(cellIndex(col, row))
        }

        if (moves.length)
            positioner.move(moves)
    }

    Connections {
        target: positioner.enabled ? dirModel : null

        function onMove(x, y, urls) {
            control.moveToCell(x, y, urls,
                               dirModel.dragging ? control.dragAnchorIndex : -1,
                               control.dragGrabOffset)
        }
    }

    Loader {
        z: 1
        width: control.width
        height: control.topInset
        active: control.topInset > 0

        sourceComponent: FishUI.HeaderBackdrop {
            view: control
            baseColor: FishUI.Theme.secondBackgroundColor
        }
    }

    DragDrop.DropArea {
        id: dropArea
        parent: control.interactionSurface
        anchors.fill: parent
        visible: control.visible

        onDragEnter: function(event) {
            var pos = mapToItem(control, event.x, event.y)
            control.updateDragScrollPosition(pos.y)
            control.updateDropTarget(event, pos)
            event.accept(Qt.MoveAction)
        }

        onDragMove: function(event) {
            var pos = mapToItem(control, event.x, event.y)
            control.updateDragScrollPosition(pos.y)
            control.updateDropTarget(event, pos)
            event.accept(Qt.MoveAction)
        }

        onDragLeave: {
            control.stopDragScroll()
            control.clearDropTarget()
        }

        onDrop: function(event) {
            control.stopDragScroll()
            control.drop(control, event, mapToItem(control, event.x, event.y))
        }
    }

    // Drag move events stop when the pointer stays at an edge, so keep scrolling
    // from the last position until the drag leaves the view. Stepping once per
    // frame keeps the scroll from juddering against the display.
    FrameAnimation {
        running: control.dragScrollActive && control.dragScrollDirection !== 0
                 && !control.isDesktopView
        onTriggered: control.autoScrollDuringDrag(frameTime)
    }

    MouseArea {
        id: _mouseArea
        parent: control.interactionSurface
        anchors.fill: parent
        propagateComposedEvents: true
        preventStealing: true
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        hoverEnabled: true
        enabled: true
        z: -1

        onPressed: function(mouse) {
            var pos = mapToItem(control, mouse.x, mouse.y)
            control.forceActiveFocus()

            if (control.editor && !control.editor.commit()) {
                mouse.accepted = true
                return
            }

            // Modifiers pressed while another window had focus never reach Keys.
            control.ctrlPressed = (mouse.modifiers & Qt.ControlModifier)
            control.shiftPressed = (mouse.modifiers & Qt.ShiftModifier)

            control.pressActive = true
            pressX = pos.x
            pressY = pos.y

            // A press on a hovered item makes it the pressed item.
            if (hoveredItem) {
                pressedItem = hoveredItem

                // Moving currentIndex here would move the anchor of a Shift range.
                if (!control.shiftPressed)
                    currentIndex = pressedItem.index

                // Shift selects a range.
                if (control.shiftPressed && control.currentIndex !== -1) {
                    positioner.setRangeSelected(control.anchorIndex, hoveredItem.index)
                } else {
                    // Ctrl keeps the rest of the selection.
                    if (!control.ctrlPressed && !dirModel.isSelected(positioner.map(hoveredItem.index))) {
                        dirModel.clearSelection()
                    }

                    if (control.ctrlPressed) {
                        dirModel.toggleSelected(positioner.map(hoveredItem.index))
                    } else {
                        dirModel.setSelected(positioner.map(hoveredItem.index))
                    }
                }

                // Item context menu.
                if (mouse.buttons & Qt.RightButton) {
                    clearPressState()
                    control.openContextMenu(mouse.modifiers, pos.x, pos.y)
                    mouse.accepted = true
                }
            } else {
                // A press on empty space clears the selection, unless a band
                // started with a modifier is to build on it.
                if (!control.ctrlPressed && !control.shiftPressed) {
                    control.currentIndex = -1
                    dirModel.clearSelection()
                }

                // Folder context menu.
                if (mouse.buttons & Qt.RightButton) {
                    clearPressState()
                    control.openContextMenu(mouse.modifiers, pos.x, pos.y)
                    mouse.accepted = true
                }
            }
        }

        onPositionChanged: function(mouse) {
            var pos = mapToItem(control, mouse.x, mouse.y)
            control.lastMouseX = pos.x
            control.lastMouseY = pos.y
            control.ctrlPressed = (mouse.modifiers & Qt.ControlModifier)
            control.shiftPressed = (mouse.modifiers & Qt.ShiftModifier)

            // No hover highlight follows the pointer while the band is out.
            if (control.rubberBand) {
                control.updateRubberBand(pos.x, pos.y)
                control.updateDragScrollPosition(pos.y)
                return
            }

            var index = control.indexAt(pos.x + control.contentX,
                                        pos.y + control.contentY)
            var indexItem = control.itemAtIndex(index)

            // Set hoveredItem.
            if (indexItem) {
                var fPos = control.mapToItem(indexItem, pos.x, pos.y)

                if (indexItem.containsPoint(fPos.x, fPos.y)) {
                    control.hoveredItem = indexItem
                } else {
                    control.hoveredItem = null
                }
            } else {
                control.hoveredItem = null
            }

            if (control.pressActive
                    && Math.abs(pos.x - pressX) < Qt.styleHints.startDragDistance
                    && Math.abs(pos.y - pressY) < Qt.styleHints.startDragDistance)
                return

            if (control.pressActive) {
                if (pressedItem != null && dirModel.isSelected(positioner.map(pressedItem.index))) {
                    control.dragX = pos.x
                    control.dragY = pos.y
                    control.verticalDropHitscanOffset = pressedItem.y + (pressedItem.height / 2)

                    var grab = control.mapToItem(pressedItem, pos.x, pos.y)
                    control.dragAnchorIndex = pressedItem.index
                    control.dragGrabOffset = Qt.point(grab.x, grab.y)

                    control.refreshDragImages()
                    dirModel.dragSelected(pos.x, pos.y)
                    control.dragX = -1
                    control.dragY = -1
                    clearPressState()
                } else {
                    if (control.editor && control.editor.targetItem)
                        return

                    dirModel.pinSelection()
                    control.hoveredItem = null
                    control.rubberBand = rubberBandObject.createObject(control.contentItem, {x: cPress.x, y: cPress.y})
                    // Otherwise the band first shows on the next move, a step behind.
                    control.updateRubberBand(pos.x, pos.y)
                    control.updateDragScrollPosition(pos.y)
                }
            }
        }

        onClicked: {
            clearPressState()
        }

        onDoubleClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton && control.pressedItem)
                dirModel.openSelected()
        }

        onReleased: {
            // A plain click inside a multiple selection narrows it to that item.
            if (pressedItem != null &&
                    !control.rubberBand &&
                    !control.shiftPressed &&
                    !control.ctrlPressed &&
                    !dirModel.dragging) {
                dirModel.clearSelection()
                dirModel.setSelected(positioner.map(pressedItem.index))
            }

            pressCanceled()
        }

        onCanceled: pressCanceled()

        onWheel: function(wheel) {
            if (wheel.modifiers & Qt.ControlModifier) {
                if (wheel.angleDelta.y > 0)
                    control.increaseIconSize()
                else
                    control.decreaseIconSize()
            } else {
                wheel.accepted = false
            }
        }
    }

    // The drag pixmap is assembled from snapshots the delegates take when they
    // are selected. An icon that has since been moved to another cell still
    // carries its old geometry, which would drag the picture far off the cursor,
    // so re-sync every selected icon just before the drag starts.
    function refreshDragImages() {
        // The snapshots are in content coordinates, the drag cursor is not.
        dirModel.setDragHotSpotScrollOffset(control.contentX, control.contentY)

        var delegates = control.contentItem.children

        for (var i = 0; i < delegates.length; ++i) {
            var item = delegates[i]

            if (typeof item.updateDragImage !== "function" || item.blank || !item.selected)
                continue

            if (!dirModel.updateItemDragRect(positioner.map(item.index), item.x, item.y, item.width, item.height))
                item.updateDragImage()
        }
    }

    function clearPressState() {
        pressActive = false
        pressedItem = null
        pressX = -1
        pressY = -1
    }

    function pressCanceled() {
        control.stopDragScroll()

        if (control.rubberBand) {
            control.rubberBand.close()
            control.rubberBand = null

            control.cachedRectangleSelection = null
            control.cachedRectangleSelectionMode = ""
            dirModel.unpinSelection()

            var delegates = control.contentItem.children

            for (var i = 0; i < delegates.length; ++i) {
                var item = delegates[i]

                if (typeof item.updateDragImage === "function" && item.selected)
                    item.updateDragImage()
            }
        }

        clearPressState()
    }

    function rectangleSelect(x, y, width, height) {
        var rows = (control.flow === GridView.FlowLeftToRight)
        var axis = rows ? control.width : control.height
        var step = rows ? cellWidth : cellHeight
        var perStripe = Math.max(1, Math.floor(axis / step))
        var stripes = Math.ceil(control.count / perStripe)
        var cWidth = control.cellWidth - (2 * FishUI.Units.smallSpacing)
        var cHeight = control.cellHeight - (2 * FishUI.Units.smallSpacing)
        var indices = []

        var logicalX = x

        if (control.effectiveLayoutDirection === Qt.RightToLeft) {
            logicalX = (rows ? control.width : control.contentItem.width)
                    + (rows ? control.contentX : control.originX) - cWidth - x - width
        }

        var firstColumn = Math.max(0, Math.floor(logicalX / control.cellWidth) - 1)
        var lastColumn = Math.floor((logicalX + width) / control.cellWidth) + 1
        var firstRow = Math.max(0, Math.floor(y / control.cellHeight) - 1)
        var lastRow = Math.floor((y + height) / control.cellHeight) + 1
        var firstStripe = rows ? firstRow : firstColumn
        var lastStripe = Math.min(stripes - 1, rows ? lastRow : lastColumn)
        var firstCell = rows ? firstColumn : firstRow
        var lastCell = Math.min(perStripe - 1, rows ? lastColumn : lastRow)

        for (var s = firstStripe; s <= lastStripe; s++) {
            for (var i = firstCell; i <= lastCell; i++) {
                var index = (s * perStripe) + i

                if (index >= control.count) {
                    break
                }

                if (positioner.isBlank(index)) {
                    continue
                }

                var itemX = ((rows ? i : s) * control.cellWidth)
                var itemY = ((rows ? s : i) * control.cellHeight)

                if (control.effectiveLayoutDirection === Qt.RightToLeft) {
                    itemX -= (rows ? control.contentX : control.originX)
                    itemX += cWidth
                    itemX = (rows ? control.width : control.contentItem.width) - itemX
                }

                // Check the cell first to avoid doing more expensive work.
                var cellX = itemX + FishUI.Units.smallSpacing
                var cellY = itemY + FishUI.Units.smallSpacing

                if (cellX >= x + width || x >= cellX + cWidth
                        || cellY >= y + height || y >= cellY + cHeight) {
                    continue
                }

                // A delegate answers for its icon and name, which feels better
                // than the whole cell; one outside the cache only has its cell.
                var item = control.itemAtIndex(index)

                if (!item || item.intersectsRect(x - item.x, y - item.y, width, height)) {
                    indices.push(index)
                }
            }
        }

        control.applyRectangleSelection(indices)
    }

    function increaseIconSize() {
        if (iconSize >= control.maximumIconSize) {
            iconSize = control.maximumIconSize
            return
        }

        iconSize = Math.min(control.maximumIconSize, iconSize + (isDesktopView ? 8 : iconSize * 0.1))
    }

    function decreaseIconSize() {
        if (iconSize <= control.minimumIconSize) {
            iconSize = control.minimumIconSize
            return
        }

        iconSize = Math.max(control.minimumIconSize, iconSize - (isDesktopView ? 8 : iconSize * 0.1))
    }

    function updateSelection(modifier) {
        if (modifier & Qt.ShiftModifier) {
            positioner.setRangeSelected(anchorIndex, currentIndex)
        } else {
            dirModel.clearSelection()
            dirModel.setSelected(positioner.map(currentIndex))
        }
    }

    Component {
        id: editorComponent

        RenameEditor {
            view: control
            centered: true
            onSubmitted: function(name) {
                dirModel.rename(positioner.map(targetItem.index), name)
            }
            onFinished: {
                control.editor = null
                destroy()
            }
        }
    }
}
