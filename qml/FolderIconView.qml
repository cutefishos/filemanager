import QtQuick 2.12
import QtQuick.Controls 2.12

import MeuiKit 1.0 as Meui
import Cutefish.FileManager 1.0 as FM

GridView {
    id: control

    // XXXX
    property var iconSize: 130 + Meui.Units.largeSpacing

    cellWidth: {
        var extraWidth = calcExtraSpacing(iconSize, control.width - leftMargin - rightMargin);
        return iconSize + extraWidth;
    }

    cellHeight: {
        var extraHeight = calcExtraSpacing(iconSize, control.height - topMargin - bottomMargin);
        return iconSize + extraHeight;
    }

    ScrollBar.vertical: ScrollBar {}
    clip: true

    property Item rubberBand: null

    property Item hoveredItem: null
    property Item pressedItem: null

    property int pressX: -1
    property int pressY: -1
    property int dragX: -1
    property int dragY: -1
    property variant cPress: null
    property bool doubleClickInProgress: false

    property int anchorIndex: 0
    property bool ctrlPressed: false
    property bool shiftPressed: false

    property bool overflowing: (visibleArea.heightRatio < 1.0 || visibleArea.widthRatio < 1.0)

    property bool scrollLeft: false
    property bool scrollRight: false
    property bool scrollUp: false
    property bool scrollDown: false

    property variant cachedRectangleSelection: null
    property int previouslySelectedItemIndex: -1
    property int verticalDropHitscanOffset: 0

    flow: GridView.FlowLeftToRight

    currentIndex: -1

    onPressXChanged: {
        cPress = mapToItem(control.contentItem, pressX, pressY);
    }

    onPressYChanged: {
        cPress = mapToItem(control.contentItem, pressX, pressY);
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        // propagateComposedEvents: true
        hoverEnabled: true
        z: -1

        onPressed: {
            control.forceActiveFocus()

            if (mouse.source === Qt.MouseEventSynthesizedByQt) {
                var index = control.indexAt(mouse.x, mouse.y)
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
                    control.currentIndex = -1;
                    previouslySelectedItemIndex = -1;
                    dirModel.clearSelection();
                }

                if (mouse.buttons & Qt.RightButton) {
                    clearPressState()
                    dirModel.openContextMenu(null, mouse.modifiers)
                    mouse.accepted = true
                }
            } else {
                pressedItem = hoveredItem;

                if (control.shiftPressed && control.currentIndex !== -1) {
                    positioner.setRangeSelected(control.anchorIndex, hoveredItem.index);
                } else {
                    if (!control.ctrlPressed && !dirModel.isSelected(positioner.map(hoveredItem.index))) {
                        previouslySelectedItemIndex = -1;
                        dirModel.clearSelection();
                    }

                    if (control.ctrlPressed) {
                        dirModel.toggleSelected(positioner.map(hoveredItem.index));
                    } else {
                        dirModel.setSelected(positioner.map(hoveredItem.index));
                    }

                    control.currentIndex = hoveredItem.index;

                    if (mouse.buttons & Qt.RightButton) {
                        clearPressState();

                        dirModel.openContextMenu(null, mouse.modifiers);
                        mouse.accepted = true;
                    }
                }
            }
        }

        onPositionChanged: {
            control.ctrlPressed = (mouse.modifiers & Qt.ControlModifier);
            control.shiftPressed = (mouse.modifiers & Qt.ShiftModifier);

            var cPos = mapToItem(control.contentItem, mouse.x, mouse.y);
            var item = control.itemAt(cPos.x, cPos.y);
            var leftEdge = Math.min(control.contentX, control.originX);

            if (!item || item.blank) {
                if (control.hoveredItem && !root.containsDrag) {
                    control.hoveredItem = null;
                }
            } else {
                var fPos = mapToItem(item, mouse.x, mouse.y);

                if (fPos.x < 0 || fPos.y < 0 || fPos.x > item.width || fPos.y > item.height) {
                    control.hoveredItem = null;
                } else {
                    control.hoveredItem = item
                }
            }

            // Trigger autoscroll.
            if (pressX != -1) {
                control.scrollLeft = (mouse.x <= 0 && control.contentX > leftEdge);
                control.scrollRight = (mouse.x >= control.width
                    && control.contentX < control.contentItem.width - control.width);
                control.scrollUp = (mouse.y <= 0 && control.contentY > 0);
                control.scrollDown = (mouse.y >= control.height
                    && control.contentY < control.contentItem.height - control.height);
            }

            // Update rubberband geometry.
            if (control.rubberBand) {
                var rB = control.rubberBand;

                if (cPos.x < cPress.x) {
                    rB.x = Math.max(leftEdge, cPos.x);
                    rB.width = Math.abs(rB.x - cPress.x);
                } else {
                    rB.x = cPress.x;
                    var ceil = Math.max(control.width, control.contentItem.width) + leftEdge;
                    rB.width = Math.min(ceil - rB.x, Math.abs(rB.x - cPos.x));
                }

                if (cPos.y < cPress.y) {
                    rB.y = Math.max(0, cPos.y);
                    rB.height = Math.abs(rB.y - cPress.y);
                } else {
                    rB.y = cPress.y;
                    var ceil = Math.max(control.height, control.contentItem.height);
                    rB.height = Math.min(ceil - rB.y, Math.abs(rB.y - cPos.y));
                }

                // Ensure rubberband is at least 1px in size or else it will become
                // invisible and not match any items.
                rB.width = Math.max(1, rB.width);
                rB.height = Math.max(1, rB.height);

                control.rectangleSelect(rB.x, rB.y, rB.width, rB.height);

                return;
            }

            // Drag initiation.
            if (pressX != -1 /*&& root.isDrag(pressX, pressY, mouse.x, mouse.y)*/) {
                if (pressedItem != null && dirModel.isSelected(positioner.map(pressedItem.index))) {
                    dragX = mouse.x;
                    dragY = mouse.y;
                    control.verticalDropHitscanOffset = pressedItem.iconArea.y + (pressedItem.iconArea.height / 2)
                    dirModel.dragSelected(mouse.x, mouse.y);
                    dragX = -1;
                    dragY = -1;
                    clearPressState();
                } else {
                    dirModel.pinSelection();
                    control.rubberBand = rubberBandObject.createObject(control.contentItem, {x: cPress.x, y: cPress.y})
                    control.interactive = false;
                }
            }
        }

        onContainsMouseChanged: {
            if (!containsMouse && !control.rubberBand) {
                clearPressState();

                if (control.hoveredItem) {
                    control.hoveredItem = null;
                }
            }
        }

        onCanceled: pressCanceled()
        onReleased: pressCanceled()
    }

    function calcExtraSpacing(cellSize, containerSize) {
        var availableColumns = Math.floor(containerSize / cellSize);
        var extraSpacing = 0;
        if (availableColumns > 0) {
            var allColumnSize = availableColumns * cellSize;
            var extraSpace = Math.max(containerSize - allColumnSize, 0);
            extraSpacing = extraSpace / availableColumns;
        }
        return Math.floor(extraSpacing);
    }

    function clearPressState() {
        pressedItem = null;
        pressX = -1;
        pressY = -1;
    }

    function rectangleSelect(x, y, width, height) {
         var rows = (control.flow === GridView.FlowLeftToRight);
         var axis = rows ? control.width : control.height;
         var step = rows ? cellWidth : cellHeight;
         var perStripe = Math.floor(axis / step);
         var stripes = Math.ceil(control.count / perStripe);
         var cWidth = control.cellWidth - (2 * Meui.Units.smallSpacing);
         var cHeight = control.cellHeight - (2 * Meui.Units.smallSpacing);
         var midWidth = control.cellWidth / 2;
         var midHeight = control.cellHeight / 2;
         var indices = [];

         for (var s = 0; s < stripes; s++) {
             for (var i = 0; i < perStripe; i++) {
                 var index = (s * perStripe) + i;

                 if (index >= control.count) {
                     break;
                 }

                 if (positioner.isBlank(index)) {
                     continue;
                 }

                 var itemX = ((rows ? i : s) * control.cellWidth);
                 var itemY = ((rows ? s : i) * control.cellHeight);

                 if (control.effectiveLayoutDirection == Qt.RightToLeft) {
                     itemX -= (rows ? control.contentX : control.originX);
                     itemX += cWidth;
                     itemX = (rows ? control.width : control.contentItem.width) - itemX;
                 }

                 // Check if the rubberband intersects this cell first to avoid doing more
                 // expensive work.
                 if (control.rubberBand.intersects(Qt.rect(itemX + Meui.Units.smallSpacing, itemY + Meui.Units.smallSpacing,
                     cWidth, cHeight))) {
                     var item = control.contentItem.childAt(itemX + midWidth, itemY + midHeight);

                     // If this is a visible item, check for intersection with the actual
                     // icon or label rects for better feel.
                     if (item && item.iconArea) {
                         var iconRect = Qt.rect(itemX + item.iconArea.x, itemY + item.iconArea.y,
                             item.iconArea.width, item.iconArea.height);

                         if (control.rubberBand.intersects(iconRect)) {
                             indices.push(index);
                             continue;
                         }

                         var labelRect = Qt.rect(itemX + item.labelArea.x, itemY + item.labelArea.y,
                             item.labelArea.width, item.labelArea.height);

                         if (control.rubberBand.intersects(labelRect)) {
                             indices.push(index);
                             continue;
                         }
                     } else {
                         // Otherwise be content with the cell intersection.
                         indices.push(index);
                     }
                 }
             }
         }

         control.cachedRectangleSelection = indices;
    }

    onCachedRectangleSelectionChanged: {
        if (cachedRectangleSelection == null) {
            return;
        }

        if (cachedRectangleSelection.length) {
            // Set current index to start of selection.
            // cachedRectangleSelection is pre-sorted.
            currentIndex = cachedRectangleSelection[0];
        }

        dirModel.updateSelection(cachedRectangleSelection.map(positioner.map), control.ctrlPressed);
    }

    function pressCanceled() {
        if (control.rubberBand) {
            control.rubberBand.close()
            control.rubberBand = null

            control.interactive = true;
            control.cachedRectangleSelection = null;
            dirModel.unpinSelection();
        }

        clearPressState();
        control.cancelAutoscroll();
    }

    function cancelAutoscroll() {
        scrollLeft = false;
        scrollRight = false;
        scrollUp = false;
        scrollDown = false;
    }

    Component {
        id: rubberBandObject

        FM.RubberBand {
            id: rubberBand

            width: 0
            height: 0
            z: 99999
            color: Meui.Theme.highlightColor

            function close() {
                opacityAnimation.restart()
            }

            OpacityAnimator {
                id: opacityAnimation
                target: rubberBand
                to: 0
                from: 1
                duration: 150

                easing {
                    bezierCurve: [0.4, 0.0, 1, 1]
                    type: Easing.Bezier
                }

                onFinished: {
                    rubberBand.visible = false
                    rubberBand.enabled = false
                    rubberBand.destroy()
                }
            }
        }
    }
}
