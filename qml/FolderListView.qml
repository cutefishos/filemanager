import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import MeuiKit 1.0 as Meui
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

    property var itemHeight: Meui.Units.fontMetrics.height * 2 + Meui.Units.largeSpacing

    property variant cachedRectangleSelection: null

    signal keyPress(var event)

    currentIndex: -1
    clip: true

    ScrollBar.vertical: ScrollBar { }

    highlightMoveDuration: 0
    keyNavigationEnabled : true
    keyNavigationWraps : true
    Keys.onPressed: {
        if (event.key === Qt.Key_Control) {
            ctrlPressed = true
        } else if (event.key === Qt.Key_Shift) {
            shiftPressed = true

            if (currentIndex != -1)
                anchorIndex = currentIndex
        }

        control.keyPress(event)
    }
    Keys.onReleased: {
        if (event.key === Qt.Key_Control) {
            ctrlPressed = false
        } else if (event.key === Qt.Key_Shift) {
            shiftPressed = false
            anchorIndex = 0
        }
    }

    onCachedRectangleSelectionChanged: {
        if (cachedRectangleSelection === null)
            return

        if (cachedRectangleSelection.length)
            control.currentIndex[0]

        folderModel.updateSelection(cachedRectangleSelection, control.ctrlPressed)
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
            control.editor = null
        }
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
                folderModel.openSelected()
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
                    folderModel.clearSelection()
                }

                if (mouse.buttons & Qt.RightButton) {
                    clearPressState()
                    folderModel.openContextMenu(null, mouse.modifiers)
                    mouse.accepted = true
                }
            } else {
                pressedItem = hoveredItem

                if (control.shiftPressed && control.currentIndex !== -1) {
                    folderModel.setRangeSelected(control.anchorIndex, hoveredItem.index)
                } else {
                    if (!control.ctrlPressed && !folderModel.isSelected(hoveredItem.index)) {
                        previouslySelectedItemIndex = -1
                        folderModel.clearSelection()
                    }

                    if (control.ctrlPressed)
                        folderModel.toggleSelected(hoveredItem.index)
                    else
                        folderModel.setSelected(hoveredItem.index)
                }

                control.currentIndex = hoveredItem.index

                if (mouse.buttons & Qt.RightButton) {
                    clearPressState()
                    folderModel.openContextMenu(null, mouse.modifiers)
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
            var item = control.itemAt(mouse.x, mouse.y + control.contentY)
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
                if (pressedItem != null && folderModel.isSelected(pressedItem.index)) {
                    control.dragX = mouse.x
                    control.dragY = mouse.y
                    control.verticalDropHitscanOffset = pressedItem.y + (pressedItem.height / 2)
                    folderModel.dragSelected(mouse.x, mouse.y)
                    control.dragX = -1
                    control.dragY = -1
                    clearPressState()
                } else {
                    folderModel.pinSelection()
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

        onReleased: pressCanceled()
        onCanceled: pressCanceled()
    }

    function pressCanceled() {
        if (control.rubberBand) {
            control.rubberBand.close()
            control.rubberBand = null

            control.interactive = true
            control.cachedRectangleSelection = null
            folderModel.unpinSelection()
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
            const index = control.indexAt(x, i)
            if(!indexes.includes(index) && index > -1 && index< control.count)
                indexes.push(index)
        }
        cachedRectangleSelection = indexes
    }

    function updateSelection(modifier) {
        if (modifier & Qt.ShiftModifier) {
            folderModel.setRangeSelected(anchorIndex, hoveredItem)
        } else {
            folderModel.clear()
            folderModel.setSelected(currentIndex)
            if (currentIndex == -1)
                previouslySelectedItemIndex = -1
            previouslySelectedItemIndex = currentIndex
        }
    }

    Component {
        id: editorComponent

        TextArea {
            id: _editor
            visible: false
            wrapMode: Text.NoWrap
            textMargin: 0
            verticalAlignment: TextEdit.AlignVCenter
            leftPadding: 0

            property Item targetItem: null

            background: Item {}

            onTargetItemChanged: {
                if (targetItem != null) {
                    var pos = control.mapFromItem(targetItem, targetItem.labelArea.x, targetItem.labelArea.y)
                    width = targetItem.labelArea.width
                    height = targetItem.height
                    x = control.mapFromItem(targetItem.labelArea, 0, 0).x
                    y = pos.y
                    text = targetItem.labelArea.text
                    targetItem.labelArea.visible = false
                    targetItem.labelArea2.visible = false
                    _editor.select(0, folderModel.fileExtensionBoundary(targetItem.index))
                    visible = true
                } else {
                    x: 0
                    y: 0
                    visible = false
                }
            }

            Keys.onPressed: {
                switch (event.key) {
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    commit()
                    break
                case Qt.Key_Escape:
                    cancel()
                    break
                }
            }

            onVisibleChanged: {
                if (visible)
                    _editor.forceActiveFocus()
                else
                    control.forceActiveFocus()
            }

            function commit() {
                if (targetItem) {
                    targetItem.labelArea.visible = true
                    targetItem.labelArea2.visible = true
                    folderModel.rename(targetItem.index, text)
                    control.currentIndex = targetItem.index
                    targetItem = null
                }
            }

            function cancel() {
                if (targetItem) {
                    targetItem.labelArea.visible = true
                    targetItem.labelArea2.visible = true
                    targetItem = null
                }
            }
        }
    }
}
