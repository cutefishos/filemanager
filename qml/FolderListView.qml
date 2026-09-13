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

import QtQuick 2.15
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

    property int verticalDropHitscanOffset: 0
    property real dragPositionY: -1
    property int dragScrollDirection: 0
    property int dragScrollElapsed: 0
    property real dragScrollVelocity: 0
    property bool dragScrollActive: false

    readonly property real dragScrollEdge: 48
    readonly property int dragScrollRamp: 180
    readonly property real dragScrollMaxSpeed: 560
    readonly property int dragScrollInterval: 16

    property int pressX: -1
    property int pressY: -1

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
    readonly property real headerHeight: columns.headerHeight

    property real lastMouseX: -1
    property real lastMouseY: -1

    property variant cachedRectangleSelection: null

    signal keyPress(var event)
    signal contextMenuRequested()

    clip: true
    cacheBuffer: width
    reuseItems: true

    ScrollBar.vertical: ScrollBar {
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

        for (var i = 0; i < control.count; ++i) {
            var item = control.itemAtIndex(i)
            if (!item || item.blank || !item.selected)
                continue

            if (!dirModel.updateItemDragRect(item.index, item.x, item.y, item.width, item.height))
                item.updateDragImage()
        }
    }

    function updateDragScrollPosition(y) {
        dragPositionY = y
        dragScrollActive = true

        var direction = y < headerHeight + dragScrollEdge ? -1
                        : y > control.height - dragScrollEdge ? 1 : 0
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

    function autoScrollDuringDrag() {
        if (!dragScrollActive || dragScrollDirection === 0)
            return

        dragScrollElapsed += dragScrollInterval
        var minimum = control.originY - control.topMargin
        var maximum = Math.max(minimum, control.contentHeight + control.bottomMargin - control.height + control.originY)
        var distance = dragScrollDirection < 0
                ? headerHeight + dragScrollEdge - dragPositionY
                : dragPositionY - (control.height - dragScrollEdge)
        var edgeProgress = Math.max(0, Math.min(1, distance / dragScrollEdge))
        var rampProgress = Math.max(0, Math.min(1,
                                                  dragScrollElapsed / dragScrollRamp))
        var ramp = 1 - Math.pow(1 - rampProgress, 3)
        var targetVelocity = dragScrollMaxSpeed * Math.pow(edgeProgress, 1.5) * ramp

        dragScrollVelocity += (targetVelocity - dragScrollVelocity) * 0.25
        var next = control.contentY + dragScrollDirection * dragScrollVelocity * dragScrollInterval / 1000

        control.contentY = Math.max(minimum, Math.min(maximum, next))

        if (control.rubberBand)
            control.updateRubberBand(control.lastMouseX, control.lastMouseY)
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
            rB.y = Math.max(control.originY - control.topMargin, cPos.y)
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

        if (control.dragX === -1 || dragIndex !== dropIndex) {
            dirModel.drop(control, event, dropItemAt(dropPos))
        }
    }

    function dropItemAt(pos) {
        var item = control.itemAt(pos.x, pos.y)

        if (item) {
            if (item.blank) {
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

    onCachedRectangleSelectionChanged: {
        if (cachedRectangleSelection === null)
            return

        if (cachedRectangleSelection.length)
            control.currentIndex[0]

        dirModel.updateSelection(cachedRectangleSelection, control.ctrlPressed)
    }

    onContentXChanged: {
        cancelRename()
    }

    onContentYChanged: {
        cancelRename()

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

    // Rows scroll beneath the header, so it blocks input over itself.
    FileList.Header {
        z: 2
        width: control.width
        columns: control.columns
        leftInset: control.leftMargin
        rightInset: control.rightMargin
    }

    DragDrop.DropArea {
        id: dropArea
        anchors.fill: parent

        onDragEnter: function(event) {
            control.updateDragScrollPosition(event.y)
        }

        onDragMove: function(event) {
            control.updateDragScrollPosition(event.y)
        }

        onDragLeave: control.stopDragScroll()

        onDrop: function(event) {
            control.stopDragScroll()
            control.drop(control, event, mapToItem(control, event.x, event.y))
        }
    }

    // Drag move events stop when the pointer stays at an edge, so keep scrolling
    // from the last position until the drag leaves the view.
    Timer {
        interval: control.dragScrollInterval
        repeat: true
        running: control.dragScrollActive
        onTriggered: control.autoScrollDuringDrag()
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

            pressX = mouse.x
            pressY = mouse.y

            if (!hoveredItem || hoveredItem.blank) {
                if (!control.ctrlPressed) {
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

            var item = mouse.y < control.headerHeight
                    ? null : control.itemAt(mouse.x - control.leftMargin, mouse.y + control.contentY)

            if (!item || item.blank) {
                if (control.hoveredItem) {
                    control.hoveredItem = null
                }
            } else {
                control.hoveredItem = item
            }

            if (control.rubberBand) {
                control.updateRubberBand(mouse.x, mouse.y)
                control.updateDragScrollPosition(mouse.y)
                return
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
                    control.rubberBand = rubberBandObject.createObject(control.contentItem, {x: cPress.x, y: cPress.y})
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
            dirModel.unpinSelection()

            for (var i = 0; i < control.count; ++i) {
                var item = control.itemAtIndex(i)
                if (item && item.selected)
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
        cachedRectangleSelection = indexes
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
