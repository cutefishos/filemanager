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

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0

ListView {
    id: control

    property Item rubberBand: null
    property Item hoveredItem: null
    property Item pressedItem: null

    property int verticalDropHitscanOffset: 0

    property int pressX: -1
    property int pressY: -1

    property int dragX: -1
    property int dragY: -1

    property bool ctrlPressed: false
    property bool shiftPressed: false

    property int previouslySelectedItemIndex: -1
    property variant cPress: null
    property Item editor: null
    property int anchorIndex: 0

    property var itemHeight: FishUI.Units.fontMetrics.height * 2 + FishUI.Units.smallSpacing

    property variant cachedRectangleSelection: null

    signal keyPress(var event)

    currentIndex: -1
    clip: true
    cacheBuffer: width

    ScrollBar.vertical: ScrollBar { }
    boundsBehavior: Flickable.StopAtBounds

    FishUI.WheelHandler {
        target: control
    }

    function rename() {
        if (currentIndex !== -1) {
            var renameAction = control.model.action("rename")
            if (renameAction && !renameAction.enabled)
                return

            if (!control.editor)
                control.editor = editorComponent.createObject(control)

            control.editor.targetItem = control.currentItem
        }
    }

    function cancelRename() {
        if (control.editor) {
            control.editor.cancel()
            control.editor.destroy()
            control.editor = null
        }
    }

    function reset() {
        currentIndex = -1
        anchorIndex = 0
        previouslySelectedItemIndex = -1
        cancelRename()
        hoveredItem = null
        pressedItem = null
        cPress = null
    }

    highlightMoveDuration: 0
    Keys.enabled: true
    Keys.onPressed: {
        control.keyPress(event)

        if (event.key === Qt.Key_Control) {
            ctrlPressed = true
        } else if (event.key === Qt.Key_Shift) {
            shiftPressed = true

            if (currentIndex != -1)
                anchorIndex = currentIndex
        }
    }

    Keys.onReleased: {
        if (event.key === Qt.Key_Control) {
            ctrlPressed = false
        } else if (event.key === Qt.Key_Shift) {
            shiftPressed = false
            anchorIndex = 0
        }
    }

    Keys.onEscapePressed: {
        if (!editor || !editor.targetItem) {
            previouslySelectedItemIndex = -1
            dirModel.clearSelection()
            event.accepted = false
        }
    }

    Keys.onUpPressed: {
        if (!editor || !editor.targetItem) {
            var newIndex = currentIndex
            newIndex--;

            if (newIndex < 0)
                newIndex = 0

            currentIndex = newIndex
            updateSelection(event.modifiers)
        }
    }

    Keys.onDownPressed: {
        if (!editor || !editor.targetItem) {
            var newIndex = currentIndex
            newIndex++

            if (newIndex >= control.count)
                return

            currentIndex = newIndex
            updateSelection(event.modifiers)
        }
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
    }

    onPressXChanged: {
        cPress = mapToItem(control.contentItem, pressX, pressY)
    }

    onPressYChanged: {
        cPress = mapToItem(control.contentItem, pressX, pressY)
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

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton && control.pressedItem)
                dirModel.openSelected()
        }

        onPressed: {
            control.forceActiveFocus()

            if (control.editor && childAt(mouse.x, mouse.y) !== control.editor)
                control.editor.commit()

            if (mouse.source === Qt.MouseEventSynthesizedByQt) {
                var index = control.indexAt(mouse.x, mouse.y + control.contentY)
                var indexItem = control.itemAtIndex(index)
                if (indexItem && indexItem.iconArea) {
                    control.currentIndex = index
                    hoveredItem = indexItem
                } else {
                    hoveredItem = null
                }
            }

            pressX = mouse.x
            pressY = mouse.y

            if (!hoveredItem || hoveredItem.blank) {
                if (!control.ctrlPressed) {
                    control.currentIndex = -1
                    control.previouslySelectedItemIndex = -1
                    dirModel.clearSelection()
                }

                if (mouse.buttons & Qt.RightButton) {
                    clearPressState()
                    dirModel.openContextMenu(null, mouse.modifiers)
                    mouse.accepted = true
                }
            } else {
                pressedItem = hoveredItem

                if (control.shiftPressed && control.currentIndex !== -1) {
                    dirModel.setRangeSelected(control.anchorIndex, hoveredItem.index)
                } else {
                    if (!control.ctrlPressed && !dirModel.isSelected(hoveredItem.index)) {
                        previouslySelectedItemIndex = -1
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
                    dirModel.openContextMenu(null, mouse.modifiers)
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

        onPositionChanged: {
            control.ctrlPressed = (mouse.modifiers & Qt.ControlModifier)
            control.shiftPressed = (mouse.modifiers & Qt.ShiftModifier)

            var cPos = mapToItem(control.contentItem, mouse.x, mouse.y)
            var item = control.itemAt(mouse.x - control.leftMargin, mouse.y + control.contentY)
            var leftEdge = Math.min(control.contentX, control.originX)

            if (!item || item.blank) {
                if (control.hoveredItem) {
                    control.hoveredItem = null
                }
            } else {
                control.hoveredItem = item
            }

            // TODO: autoscroll

            if (control.rubberBand) {
                var rB = control.rubberBand

                if (cPos.x < cPress.x) {
                    rB.x = Math.max(leftEdge, cPos.x)
                    rB.width = Math.abs(rB.x - cPress.x)
                } else {
                    rB.x = cPress.x
                    var ceil = Math.max(control.width, control.contentItem.width) + leftEdge
                    rB.width = Math.min(ceil - rB.x, Math.abs(rB.x - cPos.x))
                }

                if (cPos.y < cPress.y) {
                    rB.y = Math.max(0, cPos.y)
                    rB.height = Math.abs(rB.y - cPress.y)
                } else {
                    rB.y = cPress.y
                    var ceilValue = Math.max(control.height, control.contentItem.height)
                    rB.height = Math.min(ceilValue - rB.y, Math.abs(rB.y - cPos.y))
                }

                // Ensure rubberband is at least 1px in size or else it will become
                // invisible and not match any items.
                rB.width = Math.max(1, rB.width)
                rB.height = Math.max(1, rB.height)

                control.rectangleSelect(rB.x, rB.y, rB.width, rB.height)

                return
            }

            // Drag
            if (pressX != -1) {
                if (pressedItem != null && dirModel.isSelected(pressedItem.index)) {
                    control.dragX = mouse.x
                    control.dragY = mouse.y
                    control.verticalDropHitscanOffset = pressedItem.y + (pressedItem.height / 2)
                    dirModel.dragSelected(mouse.x, mouse.y)
                    control.dragX = -1
                    control.dragY = -1
                    clearPressState()
                } else {
                    if (control.editor && control.editor.targetItem)
                        return;

                    dirModel.pinSelection()
                    control.rubberBand = rubberBandObject.createObject(control.contentItem, {x: cPress.x, y: cPress.y})
                    control.interactive = false
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
        if (control.rubberBand) {
            control.rubberBand.close()
            control.rubberBand = null

            control.interactive = true
            control.cachedRectangleSelection = null
            dirModel.unpinSelection()
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
        for (var i = y; i <= y + height; i += 10) {
            const index = control.indexAt(control.leftMargin, i)
            if(!indexes.includes(index) && index > -1 && index < control.count)
                indexes.push(index)
        }
        cachedRectangleSelection = indexes
    }

    function updateSelection(modifier) {
        if (modifier & Qt.ShiftModifier) {
            dirModel.setRangeSelected(anchorIndex, currentIndex)
        } else {
            dirModel.clearSelection()
            dirModel.setSelected(currentIndex)
            if (currentIndex == -1)
                previouslySelectedItemIndex = -1
            previouslySelectedItemIndex = currentIndex
        }
    }

    Component {
        id: editorComponent

        TextField {
            id: _editor
            visible: false
            wrapMode: Text.NoWrap
            verticalAlignment: TextEdit.AlignVCenter
            z: 999

            background: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: FishUI.Units.smallSpacing
                    anchors.bottomMargin: FishUI.Units.smallSpacing
                    radius: FishUI.Theme.smallRadius
                    color: FishUI.Theme.backgroundColor
                }
            }

            property Item targetItem: null

            onTargetItemChanged: {
                if (targetItem != null) {
                    var pos = control.mapFromItem(targetItem, targetItem.labelArea.x, targetItem.labelArea.y)
                    width = targetItem.labelArea.width
                    height = FishUI.Units.fontMetrics.height + FishUI.Units.largeSpacing * 2
                    x = control.mapFromItem(targetItem.labelArea, 0, 0).x
                    y = pos.y + (targetItem.height - height) / 2
                    text = targetItem.labelArea.text
                    targetItem.labelArea.visible = false
                    targetItem.labelArea2.visible = false
                    _editor.select(0, dirModel.fileExtensionBoundary(targetItem.index))
                    visible = true
                    control.interactive = false
                } else {
                    x: 0
                    y: 0
                    visible = false
                    control.interactive = true
                }
            }

            onVisibleChanged: {
                if (visible)
                    _editor.forceActiveFocus()
                else
                    control.forceActiveFocus()
            }

            Keys.onPressed: {
                switch (event.key) {
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    commit()
                    event.accepted = true
                    break
                case Qt.Key_Escape:
                    cancel()
                    event.accepted = true
                    break
                }
            }

            function commit() {
                if (targetItem) {
                    targetItem.labelArea.visible = true
                    targetItem.labelArea2.visible = true
                    dirModel.rename(targetItem.index, text)
                    control.currentIndex = targetItem.index
                    targetItem = null

                    control.editor.destroy()
                }
            }

            function cancel() {
                if (targetItem) {
                    targetItem.labelArea.visible = true
                    targetItem.labelArea2.visible = true
                    control.currentIndex = targetItem.index
                    targetItem = null

                    control.editor.destroy()
                }
            }
        }
    }
}
