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
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0
import Cutefish.DragDrop 1.0 as DragDrop
import "FileList" as FileList

ListView {
    id: control

    objectName: "FolderListView"

    property Item rubberBand: null
    property Item hoveredItem: null
    property Item pressedItem: null
    property Item dropTargetItem: null
    property bool dropTargetAllowed: false

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
    property Item editor: null
    property int anchorIndex: 0
    property bool useCustomContextMenu: false

    // Named apart from the context's settings so the binding below is not circular.
    readonly property var listSettings: settings
    readonly property FileList.Columns columns: FileList.Columns {
        settings: control.listSettings
        availableWidth: Math.max(0, control.width - control.leftMargin - control.rightMargin)
    }
    readonly property real rowHeight: columns.rowHeight
    // Bars floating over the top of the view; the column header sits below them
    // and the rows scroll beneath both.
    property real topInset: 0
    readonly property real headerHeight: topInset + columns.headerHeight

    property real lastMouseX: -1
    property real lastMouseY: -1

    property variant cachedRectangleSelection: null
    property string cachedRectangleSelectionMode: ""

    signal keyPress(var event)
    signal contextMenuRequested()

    clip: true
    cacheBuffer: width
    reuseItems: true

    FishUI.ScrollBar.vertical: FishUI.ScrollBar {
        topPadding: control.headerHeight
    }
    boundsBehavior: Flickable.StopAtBounds
    // Dragging the view must rubber band select, not flick the content around
    // like a touch interface. WheelHandler and the scroll bar do the scrolling.
    interactive: false

    FishUI.WheelHandler {
        target: control
    }

    function refreshDragImages() {
        dirModel.setDragHotSpotScrollOffset(control.contentX, control.contentY)

        var delegates = control.contentItem.children

        for (var i = 0; i < delegates.length; ++i) {
            var item = delegates[i]

            if (typeof item.updateDragImage !== "function" || item.blank || !item.selected)
                continue

            if (!dirModel.updateItemDragRect(item.index, item.x, item.y, item.width, item.height))
                item.updateDragImage()
        }
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
        if (!dragScrollActive || dragScrollDirection === 0)
            return

        var interval = Math.min(50, frameTime * 1000)
        dragScrollElapsed += interval
        var minimum = control.originY - control.topMargin
        var maximum = Math.max(minimum, control.contentHeight + control.bottomMargin - control.height + control.originY)
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

    function ensureIndexVisible(index) {
        if (index < 0 || index >= control.count)
            return

        var top = index * control.rowHeight
        var visibleTop = control.contentY + control.headerHeight
        var visibleBottom = control.contentY + control.height - control.bottomMargin

        if (top < visibleTop)
            control.contentY = top - control.headerHeight
        else if (top + control.rowHeight > visibleBottom)
            control.contentY = top + control.rowHeight - control.height + control.bottomMargin
    }

    function moveCurrentIndex(newIndex, modifiers) {
        if (control.count === 0 || (editor && editor.targetItem))
            return

        newIndex = Math.max(0, Math.min(control.count - 1, newIndex))
        if (!(modifiers & Qt.ShiftModifier) || currentIndex === -1)
            anchorIndex = newIndex

        currentIndex = newIndex
        updateSelection(modifiers)
        ensureIndexVisible(newIndex)
    }

    function updateRubberBand(mouseX, mouseY) {
        var rB = control.rubberBand
        if (!rB || !cPress)
            return

        var cPos = mapToItem(control.contentItem, mouseX, mouseY)
        var leftEdge = Math.min(control.contentX, control.originX)

        if (cPos.x < cPress.x) {
            rB.x = Math.max(leftEdge, cPos.x)
            rB.width = Math.abs(rB.x - cPress.x)
        } else {
            rB.x = cPress.x
            var ceil = Math.max(control.width, control.contentItem.width) + leftEdge
            rB.width = Math.min(ceil - rB.x, Math.abs(rB.x - cPos.x))
        }

        var bottom = Math.max(control.count * control.rowHeight, control.contentY + control.height)
        if (cPos.y < cPress.y) {
            rB.y = Math.max(control.originY, cPos.y)
            rB.height = Math.abs(rB.y - cPress.y)
        } else {
            rB.y = cPress.y
            rB.height = Math.min(bottom - rB.y, Math.abs(rB.y - cPos.y))
        }

        // Ensure rubberband is at least 1px in size or else it will become
        // invisible and not match any items.
        rB.width = Math.max(1, rB.width)
        rB.height = Math.max(1, rB.height)

        control.rectangleSelect(rB.x, rB.y, rB.width, rB.height)
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
            "selectionEnd": dirModel.fileExtensionBoundary(control.currentIndex)
        })
        control.editor.begin()
    }

    function cancelRename() {
        if (control.editor)
            control.editor.cancel()
    }

    function openContextMenu(modifiers) {
        if (control.useCustomContextMenu) {
            dirModel.prepareContextMenu()
            control.contextMenuRequested()
        } else {
            dirModel.openContextMenu(null, modifiers)
        }
    }

    function reset() {
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
        dirModel.drop(control, event, row)
        control.clearDropTarget()
    }

    // The row of the folder a drop at pos goes into, or -1 for the folder
    // shown; a file's row passes the drop on to the folder around it.
    function dropItemAt(pos) {
        var item = control.itemAt(pos.x, pos.y)

        if (item) {
            if (item.blank || !item.isDir) {
                return -1
            }

            var hOffset = Math.abs(Math.min(control.contentX, control.originX))
            var hPos = mapToItem(item, pos.x + hOffset, pos.y)

            if ((hPos.x < 0 || hPos.y < 0 || hPos.x > item.width || hPos.y > item.height)) {
                return -1
            } else {
                return item.index
            }
        }

        return -1
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

    highlightMoveDuration: 0
    Keys.enabled: true
    Keys.onPressed: function(event) {
        control.keyPress(event)

        var page = Math.max(1, Math.floor((control.height - control.headerHeight) / control.rowHeight) - 1)

        if (event.key === Qt.Key_Home) {
            moveCurrentIndex(0, event.modifiers)
            event.accepted = true
        } else if (event.key === Qt.Key_End) {
            moveCurrentIndex(control.count - 1, event.modifiers)
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            moveCurrentIndex(Math.max(0, currentIndex) - page, event.modifiers)
            event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            moveCurrentIndex(currentIndex + page, event.modifiers)
            event.accepted = true
        } else if (event.key === Qt.Key_Control) {
            ctrlPressed = true
        } else if (event.key === Qt.Key_Shift) {
            shiftPressed = true

            if (currentIndex != -1)
                anchorIndex = currentIndex
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
        moveCurrentIndex(currentIndex === -1 ? 0 : currentIndex - 1, event.modifiers)
    }

    Keys.onDownPressed: function(event) {
        moveCurrentIndex(currentIndex + 1, event.modifiers)
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

        dirModel.updateSelection(indices, mode === "toggle", mode === "extend")
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

        if (_mouseArea.containsMouse && !rubberBand && lastMouseY >= control.headerHeight) {
            var item = control.itemAt(lastMouseX - control.leftMargin, lastMouseY + control.contentY)
            hoveredItem = item && !item.blank ? item : null
        }
    }

    onPressXChanged: {
        cPress = mapToItem(control.contentItem, pressX, pressY)
    }

    onPressYChanged: {
        cPress = mapToItem(control.contentItem, pressX, pressY)
    }

    Loader {
        z: 1
        width: control.width
        height: control.headerHeight
        active: control.topInset > 0

        // The column header draws its own separator.
        sourceComponent: FishUI.HeaderBackdrop {
            view: control
            separatorVisible: false
        }
    }

    // Rows scroll beneath the header, so it blocks input over itself.
    FileList.Header {
        z: 2
        y: control.topInset
        width: control.width
        backgroundColor: control.topInset > 0 ? "transparent" : FishUI.Theme.secondBackgroundColor
        columns: control.columns
        leftInset: control.leftMargin
        rightInset: control.rightMargin
    }

    DragDrop.DropArea {
        id: dropArea
        anchors.fill: parent

        onDragEnter: function(event) {
            control.updateDragScrollPosition(event.y)
            control.updateDropTarget(event, Qt.point(event.x, event.y))
            event.accept(Qt.MoveAction)
        }

        onDragMove: function(event) {
            control.updateDragScrollPosition(event.y)
            control.updateDropTarget(event, Qt.point(event.x, event.y))
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
        onTriggered: control.autoScrollDuringDrag(frameTime)
    }

    MouseArea {
        id: _mouseArea
        anchors.fill: parent
        propagateComposedEvents: true
        preventStealing: true
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        hoverEnabled: true
        enabled: true
        z: -1

        onDoubleClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton && control.pressedItem)
                dirModel.openSelected()
        }

        onPressed: function(mouse) {
            control.forceActiveFocus()

            if (control.editor && !control.editor.commit()) {
                mouse.accepted = true
                return
            }

            // Hover goes stale when the wheel scrolls under a still pointer.
            var pressed = control.itemAt(mouse.x - control.leftMargin, mouse.y + control.contentY)
            hoveredItem = pressed && !pressed.blank ? pressed : null

            // Modifiers pressed while another window had focus never reach Keys.
            control.ctrlPressed = (mouse.modifiers & Qt.ControlModifier)
            control.shiftPressed = (mouse.modifiers & Qt.ShiftModifier)

            pressX = mouse.x
            pressY = mouse.y

            if (!hoveredItem || hoveredItem.blank) {
                // A band started with a modifier builds on the selection.
                if (!control.ctrlPressed && !control.shiftPressed) {
                    control.currentIndex = -1
                    dirModel.clearSelection()
                }

                if (mouse.buttons & Qt.RightButton) {
                    clearPressState()
                    control.openContextMenu(mouse.modifiers)
                    mouse.accepted = true
                }
            } else {
                pressedItem = hoveredItem

                if (control.shiftPressed && control.currentIndex !== -1) {
                    dirModel.setRangeSelected(control.anchorIndex, hoveredItem.index)
                } else {
                    if (!control.ctrlPressed && !dirModel.isSelected(hoveredItem.index)) {
                        dirModel.clearSelection()
                    }

                    if (control.ctrlPressed) {
                        dirModel.toggleSelected(hoveredItem.index)
                    } else {
                        dirModel.setSelected(hoveredItem.index)
                    }
                }

                control.currentIndex = hoveredItem.index

                if (mouse.buttons & Qt.RightButton) {
                    clearPressState()
                    control.openContextMenu(mouse.modifiers)
                    mouse.accepted = true
                }
            }
        }

        onClicked: {
            clearPressState()

            if (!hoveredItem || hoveredItem.blank || control.currentIndex === -1 || control.ctrlPressed
                    || control.shiftPressed) {
                return
            }

            // TODO: rename
        }

        onPositionChanged: function(mouse) {
            control.ctrlPressed = (mouse.modifiers & Qt.ControlModifier)
            control.shiftPressed = (mouse.modifiers & Qt.ShiftModifier)

            control.lastMouseX = mouse.x
            control.lastMouseY = mouse.y

            // No hover highlight follows the pointer while the band is out.
            if (control.rubberBand) {
                control.updateRubberBand(mouse.x, mouse.y)
                control.updateDragScrollPosition(mouse.y)
                return
            }

            var item = mouse.y < control.headerHeight
                    ? null : control.itemAt(mouse.x - control.leftMargin, mouse.y + control.contentY)

            if (!item || item.blank) {
                if (control.hoveredItem) {
                    control.hoveredItem = null
                }
            } else {
                control.hoveredItem = item
            }

            if (pressX != -1 && Math.abs(mouse.x - pressX) < Qt.styleHints.startDragDistance
                    && Math.abs(mouse.y - pressY) < Qt.styleHints.startDragDistance)
                return

            // Drag
            if (pressX != -1) {
                if (pressedItem != null && dirModel.isSelected(pressedItem.index)) {
                    control.dragX = mouse.x
                    control.dragY = mouse.y
                    control.verticalDropHitscanOffset = pressedItem.y + (pressedItem.height / 2)
                    control.refreshDragImages()
                    dirModel.dragSelected(mouse.x, mouse.y)
                    control.dragX = -1
                    control.dragY = -1
                    clearPressState()
                } else {
                    if (control.editor && control.editor.targetItem)
                        return;

                    dirModel.pinSelection()
                    control.hoveredItem = null
                    control.rubberBand = rubberBandObject.createObject(control.contentItem, {x: cPress.x, y: cPress.y})
                    // Otherwise the band first shows on the next move, a step behind.
                    control.updateRubberBand(mouse.x, mouse.y)
                    control.updateDragScrollPosition(mouse.y)
                }
            }
        }

        onContainsMouseChanged: {
            if (!containsMouse && !control.rubberBand) {
                clearPressState()

                if (control.hoveredItem) {
                    control.hoveredItem = null
                }
            }
        }

        onReleased: {
            if (pressedItem != null &&
                    !control.rubberBand &&
                    !control.shiftPressed &&
                    !control.ctrlPressed &&
                    !dirModel.dragging) {
                dirModel.clearSelection()
                dirModel.setSelected(pressedItem.index)
            }

            pressCanceled()
        }

        onCanceled: pressCanceled()
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
        // control.cancelAutoscroll()
    }

    function clearPressState() {
        pressedItem = null
        pressX = -1
        pressY = -1
    }

    function rectangleSelect(x, y, width, height) {
        var indexes = []
        var first = Math.max(0, Math.floor(y / control.rowHeight))
        var last = Math.min(control.count - 1, Math.floor((y + height) / control.rowHeight))
        for (var i = first; i <= last; ++i)
            indexes.push(i)
        applyRectangleSelection(indexes)
    }

    function updateSelection(modifier) {
        if (modifier & Qt.ShiftModifier) {
            dirModel.setRangeSelected(anchorIndex, currentIndex)
        } else {
            dirModel.clearSelection()
            dirModel.setSelected(currentIndex)
        }
    }

    Component {
        id: editorComponent

        RenameEditor {
            view: control
            centered: false
            onSubmitted: function(name) {
                dirModel.rename(targetItem.index, name)
            }
            onFinished: {
                control.editor = null
                destroy()
            }
        }
    }
}
