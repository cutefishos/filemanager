import QtQuick 2.12
import QtQml 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Cutefish.FileManager 1.0 as FM
import MeuiKit 1.0 as Meui

import "../"
import "FolderTools.js" as FolderTools

FocusScope {
    id: control

    property QtObject model: dir
    property Item rubberBand: null

    property alias positions: positioner.positions
    property alias scrollLeft: gridView.scrollLeft
    property alias scrollRight: gridView.scrollRight
    property alias scrollUp: gridView.scrollUp
    property alias scrollDown: gridView.scrollDown
    property alias hoveredItem: gridView.hoveredItem
    property int previouslySelectedItemIndex: -1

    property alias cellWidth: gridView.cellWidth
    property alias cellHeight: gridView.cellHeight

    function positionViewAtBeginning() {
        gridView.positionViewAtBeginning();
    }

    function rename() {
        if (gridView.currentIndex != -1) {
            var renameAction = folderView.model.action("rename");
            if (renameAction && !renameAction.enabled) {
                return;
            }

            if (!editor) {
                editor = editorComponent.createObject(listener);
            }

            editor.targetItem = gridView.currentItem;
        }
    }

    function cancelRename() {
        if (editor) {
            editor.targetItem = null;
        }
    }

    function handleDragMove(x, y) {
        var child = childAt(x, y);

        if (child !== null) {
            hoveredItem = null;
        } else {
            var pos = mapToItem(gridView.contentItem, x, y);
            var item = gridView.itemAt(pos.x, pos.y);

            if (item && item.isDir) {
                hoveredItem = item;
            } else {
                hoveredItem = null;
            }
        }
    }

    function endDragMove() {
        if (hoveredItem) {
            hoveredItem = null;
        }
    }

    function dropItemAt(pos) {
        var item = gridView.itemAt(pos.x, pos.y);

        if (item) {
            if (item.blank) {
                return -1;
            }

            var hOffset = Math.abs(Math.min(gridView.contentX, gridView.originX));
            var hPos = mapToItem(item.frame, pos.x + hOffset, pos.y);

            if ((hPos.x < 0 || hPos.y < 0 || hPos.x > item.frame.width || hPos.y > item.frame.height)) {
                return -1;
            } else {
                return positioner.map(item.index);
            }
        }

        return -1;
    }

    function drop(target, event, pos) {
        var dropPos = mapToItem(gridView.contentItem, pos.x, pos.y);
        var dropIndex = gridView.indexAt(dropPos.x, dropPos.y);
        var dragPos = mapToItem(gridView.contentItem, listener.dragX, listener.dragY);
        var dragIndex = gridView.indexAt(dragPos.x, dragPos.y);

        if (listener.dragX === -1 || dragIndex !== dropIndex) {
            dir.drop(target, event, dropItemAt(dropPos), root.isContainment && !plasmoid.immutable);
        }
    }

    GlobalSettings {
        id: settings
    }

    MouseArea {
        id: listener
        anchors.fill: parent
        z: 999

        property alias hoveredItem: gridView.hoveredItem

        property Item pressedItem: null
        property int pressX: -1
        property int pressY: -1
        property int dragX: -1
        property int dragY: -1
        property variant cPress: null
        property bool doubleClickInProgress: false

        Keys.forwardTo: gridView

        hoverEnabled: true

        onPressXChanged: {
            cPress = mapToItem(gridView.contentItem, pressX, pressY);
        }

        onPressYChanged: {
            cPress = mapToItem(gridView.contentItem, pressX, pressY);
        }

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onWheel: {
            if (wheel.modifiers & Qt.ControlModifier) {
                if (wheel.angleDelta.y > 0)
                    gridView.increaseIconSize()
                else
                    gridView.decreaseIconSize()
            }
        }

        onPressed: {
            gridView.focus = true
            gridView.forceActiveFocus()

            if (mouse.source === Qt.MouseEventSynthesizedByQt) {
                var index = gridView.indexAt(mouse.x, mouse.y);
                var indexItem = gridView.itemAtIndex(index);
                if (indexItem && indexItem.iconArea) {
                    gridView.currentIndex = index;
                    hoveredItem = indexItem;
                } else {
                    hoveredItem = null;
                }
            }

            pressX = mouse.x;
            pressY = mouse.y;

            if (!hoveredItem || hoveredItem.blank) {
                if (!gridView.ctrlPressed) {
                    gridView.currentIndex = -1;
                    previouslySelectedItemIndex = -1;
                    dir.clearSelection();
                }

                if (mouse.buttons & Qt.RightButton) {
                    clearPressState();
                    dir.openContextMenu(null, mouse.modifiers);
                    mouse.accepted = true;
                }
            } else {
                pressedItem = hoveredItem;

                if (gridView.shiftPressed && gridView.currentIndex != -1) {
                    positioner.setRangeSelected(gridView.anchorIndex, hoveredItem.index);
                } else {
                    if (!gridView.ctrlPressed && !dir.isSelected(positioner.map(hoveredItem.index))) {
                        previouslySelectedItemIndex = -1;
                        dir.clearSelection();
                    }

                    if (gridView.ctrlPressed) {
                        dir.toggleSelected(positioner.map(hoveredItem.index));
                    } else {
                        dir.setSelected(positioner.map(hoveredItem.index));
                    }

                    gridView.currentIndex = hoveredItem.index;

                    if (mouse.buttons & Qt.RightButton) {
                        clearPressState();

                        dir.openContextMenu(null, mouse.modifiers);
                        mouse.accepted = true;
                    }
                }
            }

            // control.pressed();
        }

        onPositionChanged: {
            gridView.ctrlPressed = (mouse.modifiers & Qt.ControlModifier);
            gridView.shiftPressed = (mouse.modifiers & Qt.ShiftModifier);

            var cPos = mapToItem(gridView.contentItem, mouse.x, mouse.y);
            var item = gridView.itemAt(cPos.x, cPos.y);
            var leftEdge = Math.min(gridView.contentX, gridView.originX);

            if (!item || item.blank) {
                if (gridView.hoveredItem && !root.containsDrag) {
                    gridView.hoveredItem = null;
                }
            } else {
                var fPos = mapToItem(item.frame, mouse.x, mouse.y);

                if (fPos.x < 0 || fPos.y < 0 || fPos.x > item.frame.width || fPos.y > item.frame.height) {
                    gridView.hoveredItem = null;
                } else {
                    gridView.hoveredItem = item
                }
            }

            // Trigger autoscroll.
            if (pressX != -1) {
                gridView.scrollLeft = (mouse.x <= 0 && gridView.contentX > leftEdge);
                gridView.scrollRight = (mouse.x >= gridView.width
                    && gridView.contentX < gridView.contentItem.width - gridView.width);
                gridView.scrollUp = (mouse.y <= 0 && gridView.contentY > 0);
                gridView.scrollDown = (mouse.y >= gridView.height
                    && gridView.contentY < gridView.contentItem.height - gridView.height);
            }

            // Update rubberband geometry.
            if (control.rubberBand) {
                var rB = control.rubberBand;

                if (cPos.x < cPress.x) {
                    rB.x = Math.max(leftEdge, cPos.x);
                    rB.width = Math.abs(rB.x - cPress.x);
                } else {
                    rB.x = cPress.x;
                    var ceil = Math.max(gridView.width, gridView.contentItem.width) + leftEdge;
                    rB.width = Math.min(ceil - rB.x, Math.abs(rB.x - cPos.x));
                }

                if (cPos.y < cPress.y) {
                    rB.y = Math.max(0, cPos.y);
                    rB.height = Math.abs(rB.y - cPress.y);
                } else {
                    rB.y = cPress.y;
                    var ceil = Math.max(gridView.height, gridView.contentItem.height);
                    rB.height = Math.min(ceil - rB.y, Math.abs(rB.y - cPos.y));
                }

                // Ensure rubberband is at least 1px in size or else it will become
                // invisible and not match any items.
                rB.width = Math.max(1, rB.width);
                rB.height = Math.max(1, rB.height);

                gridView.rectangleSelect(rB.x, rB.y, rB.width, rB.height);

                return;
            }

            // Drag initiation.
            if (pressX != -1 && root.isDrag(pressX, pressY, mouse.x, mouse.y)) {
                if (pressedItem != null && dir.isSelected(positioner.map(pressedItem.index))) {
                    dragX = mouse.x;
                    dragY = mouse.y;
                    gridView.verticalDropHitscanOffset = pressedItem.iconArea.y + (pressedItem.iconArea.height / 2)
                    dir.dragSelected(mouse.x, mouse.y);
                    dragX = -1;
                    dragY = -1;
                    clearPressState();
                } else {
                    dir.pinSelection();
                    control.rubberBand = rubberBandObject.createObject(gridView.contentItem, {x: cPress.x, y: cPress.y})
                    gridView.interactive = false;
                }
            }
        }

        onCanceled: pressCanceled()
        onReleased: pressCanceled()

        onDoubleClicked: {
            clearPressState()

            if (!hoveredItem || hoveredItem.blank || gridView.currentIndex == -1 || gridView.ctrlPressed || gridView.shiftPressed) {
                return;
            }

            if (mouse.button === Qt.LeftButton)
                dir.runSelected()
        }

        onClicked: {
            clearPressState();

            if (!hoveredItem || hoveredItem.blank || gridView.currentIndex == -1 || gridView.ctrlPressed || gridView.shiftPressed) {
                return;
            }

            var pos = mapToItem(hoveredItem, mouse.x, mouse.y);

            // Moving from an item to its preview popup dialog doesn't unset hoveredItem
            // even though the cursor has left it, so we need to check whether the click
            // actually occurred inside the item we expect it in before going ahead. If it
            // didn't, clean up (e.g. dismissing the dialog as a side-effect of unsetting
            // hoveredItem) and abort.
            if (pos.x < 0 || pos.x > hoveredItem.width || pos.y < 0 || pos.y > hoveredItem.height) {
                hoveredItem = null;
                previouslySelectedItemIndex = -1;
                dir.clearSelection();

                return;
            // If the hoveredItem is clicked while having a preview popup dialog open,
            // only dismiss the dialog and abort.
            }
        }

        onContainsMouseChanged: {
            if (!containsMouse && !control.rubberBand) {
                clearPressState();

                if (gridView.hoveredItem) {
                    gridView.hoveredItem = null;
                }
            }
        }

        function pressCanceled() {
            if (control.rubberBand) {
                control.rubberBand.close()
                control.rubberBand = null

                gridView.interactive = true;
                gridView.cachedRectangleSelection = null;
                dir.unpinSelection();
            }

            clearPressState();
            gridView.cancelAutoscroll();
        }

        function clearPressState() {
            pressedItem = null;
            pressX = -1;
            pressY = -1;
        }
    }

    GridView {
        id: gridView
        anchors.fill: parent

        property int verticalDropHitscanOffset: 0

        property Item hoveredItem: null

        property int anchorIndex: 0
        property bool ctrlPressed: false
        property bool shiftPressed: false

        property bool overflowing: (visibleArea.heightRatio < 1.0 || visibleArea.widthRatio < 1.0)

        property bool scrollLeft: false
        property bool scrollRight: false
        property bool scrollUp: false
        property bool scrollDown: false

        property variant cachedRectangleSelection: null

        flow: GridView.FlowTopToBottom

        currentIndex: -1

        keyNavigationWraps: false
        boundsBehavior: Flickable.StopAtBounds

        focus: true

        onActiveFocusChanged: {
            if (!activeFocus) {
                dir.clearSelection()
            }
        }

        property var iconSize: settings.desktopIconSize + Meui.Units.largeSpacing

        cellWidth: {
            var extraWidth = calcExtraSpacing(iconSize, gridView.width - leftMargin - rightMargin);
            return iconSize + extraWidth;
        }

        cellHeight: {
            var extraHeight = calcExtraSpacing(iconSize, gridView.height - topMargin - bottomMargin);
            return iconSize + extraHeight;
        }

        function increaseIconSize() {
            if (iconSize >= settings.maximumIconSize) {
                iconSize = settings.maximumIconSize
                return
            }

            iconSize += (iconSize * 0.1)
            settings.desktopIconSize = iconSize
        }

        function decreaseIconSize() {
            if (iconSize <= settings.minimumIconSize) {
                iconSize = settings.minimumIconSize
                return
            }

            iconSize -= (iconSize * 0.1)
            settings.desktopIconSize = iconSize
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

        function updateSelection(modifier) {
            if (modifier & Qt.ShiftModifier) {
                positioner.setRangeSelected(anchorIndex, currentIndex);
            } else {
                dir.clearSelection();
                dir.setSelected(positioner.map(currentIndex));
                if (currentIndex == -1) {
                    previouslySelectedItemIndex = -1;
                }
                previouslySelectedItemIndex = currentIndex;
            }
        }

        leftMargin: desktopView.screenAvailableRect ? desktopView.screenAvailableRect.x : 0
        topMargin: desktopView.screenAvailableRect ? desktopView.screenAvailableRect.y : 0
        rightMargin: desktopView.screenRect.width - (desktopView.screenAvailableRect.x + desktopView.screenAvailableRect.width)
        bottomMargin: desktopView.screenRect.height - (desktopView.screenAvailableRect.y + desktopView.screenAvailableRect.height)

        delegate: FolderItemDelegate {
            width: gridView.cellWidth
            height: gridView.cellHeight
        }

        onContentXChanged: {
            // cancelRename()

            dir.setDragHotSpotScrollOffset(contentX, contentY);

            if (contentX == 0) {
                scrollLeft = false;
            }

            if (contentX == contentItem.width - width) {
                scrollRight = false;
            }

            // Update rubberband geometry.
            if (control.rubberBand) {
                var rB = control.rubberBand;

                if (scrollLeft) {
                    rB.x = Math.min(gridView.contentX, gridView.originX);
                    rB.width = listener.cPress.x;
                }

                if (scrollRight) {
                    var lastCol = gridView.contentX + gridView.width;
                    rB.width = lastCol - rB.x;
                }

                gridView.rectangleSelect(rB.x, rB.y, rB.width, rB.height);
            }
        }

        onContentYChanged: {
            // cancelRename();

            dir.setDragHotSpotScrollOffset(contentX, contentY);

            if (contentY == 0) {
                scrollUp = false;
            }

            if (contentY == contentItem.height - height) {
                scrollDown = false;
            }

            // Update rubberband geometry.
            if (control.rubberBand) {
                var rB = control.rubberBand;

                if (scrollUp) {
                    rB.y = 0;
                    rB.height = listener.cPress.y;
                }

                if (scrollDown) {
                    var lastRow = gridView.contentY + gridView.height;
                    rB.height = lastRow - rB.y;
                }

                gridView.rectangleSelect(rB.x, rB.y, rB.width, rB.height);
            }
        }

        onFlowChanged: {
            // FIXME TODO: Preserve positions.
            if (positioner.enabled) {
                positioner.reset();
            }
        }

        onLayoutDirectionChanged: {
            // FIXME TODO: Preserve positions.
            if (positioner.enabled) {
                positioner.reset();
            }
        }

        onCurrentIndexChanged: {
            positionViewAtIndex(currentIndex, GridView.Contain);
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

            dir.updateSelection(cachedRectangleSelection.map(positioner.map), gridView.ctrlPressed);
        }

        Behavior on contentX { id: smoothX; enabled: false; SmoothedAnimation { velocity: 700 } }
        Behavior on contentY { id: smoothY; enabled: false; SmoothedAnimation { velocity: 700 } }

        Keys.onPressed: {
            event.accepted = true

            if (event.matches(StandardKey.Delete)) {
                if (dir.hasSelection()) {
                    dir.action("trash").trigger();
                }
            } else if (event.key === Qt.Key_Control) {
                ctrlPressed = true;
            } else if (event.key === Qt.Key_Shift) {
                shiftPressed = true;

                if (currentIndex != -1) {
                    anchorIndex = currentIndex;
                }
            } else if (event.key === Qt.Key_Home) {
                currentIndex = 0;
                updateSelection(event.modifiers);
            } else if (event.key === Qt.Key_End) {
                currentIndex = count - 1;
                updateSelection(event.modifiers);
            } else if (event.matches(StandardKey.Copy)) {
                dir.copy();
            } else if (event.matches(StandardKey.Paste)) {
                dir.paste();
            } else if (event.matches(StandardKey.Cut)) {
                dir.cut();
            } else if (event.matches(StandardKey.Undo)) {
                dir.undo();
            } else if (event.matches(StandardKey.Refresh)) {
                dir.refresh();
            } else if (event.matches(StandardKey.SelectAll)) {
                positioner.setRangeSelected(0, count - 1);
            } else {
                event.accepted = false;
            }
        }

        Keys.onReturnPressed: {
            if (event.modifiers === Qt.AltModifier) {
                dir.openPropertiesDialog();
            } else {
                dir.runSelected()
            }
        }

        Keys.onEnterPressed: Keys.returnPressed(event)

        Keys.onReleased: {
            if (event.key === Qt.Key_Control) {
                ctrlPressed = false;
            } else if (event.key === Qt.Key_Shift) {
                shiftPressed = false;
                anchorIndex = 0;
            }
        }

        Keys.onLeftPressed: {
            if (positioner.enabled) {
                var newIndex = positioner.nearestItem(currentIndex,
                    FolderTools.effectiveNavDirection(gridView.flow, gridView.effectiveLayoutDirection, Qt.LeftArrow));

                if (newIndex !== -1) {
                    currentIndex = newIndex;
                    updateSelection(event.modifiers);
                }
            } else {
                var oldIndex = currentIndex;

                moveCurrentIndexLeft();

                if (oldIndex === currentIndex) {
                    return;
                }

                updateSelection(event.modifiers);
            }
        }

        Keys.onRightPressed: {
            if (positioner.enabled) {
                var newIndex = positioner.nearestItem(currentIndex,
                    FolderTools.effectiveNavDirection(gridView.flow, gridView.effectiveLayoutDirection, Qt.RightArrow));

                if (newIndex !== -1) {
                    currentIndex = newIndex;
                    updateSelection(event.modifiers);
                }
            } else {
                var oldIndex = currentIndex;

                moveCurrentIndexRight();

                if (oldIndex === currentIndex) {
                    return;
                }

                updateSelection(event.modifiers);
            }
        }

        Keys.onUpPressed: {
            if (positioner.enabled) {
                var newIndex = positioner.nearestItem(currentIndex,
                    FolderTools.effectiveNavDirection(gridView.flow, gridView.effectiveLayoutDirection, Qt.UpArrow));

                if (newIndex !== -1) {
                    currentIndex = newIndex;
                    updateSelection(event.modifiers);
                }
            } else {
                var oldIndex = currentIndex;

                moveCurrentIndexUp();

                if (oldIndex === currentIndex) {
                    return;
                }

                updateSelection(event.modifiers);
            }
        }

        Keys.onDownPressed: {
            if (positioner.enabled) {
                var newIndex = positioner.nearestItem(currentIndex,
                    FolderTools.effectiveNavDirection(gridView.flow, gridView.effectiveLayoutDirection, Qt.DownArrow));

                if (newIndex !== -1) {
                    currentIndex = newIndex;
                    updateSelection(event.modifiers);
                }
            } else {
                var oldIndex = currentIndex;

                moveCurrentIndexDown();

                if (oldIndex === currentIndex) {
                    return;
                }

                updateSelection(event.modifiers);
            }
        }

        function cancelAutoscroll() {
            scrollLeft = false;
            scrollRight = false;
            scrollUp = false;
            scrollDown = false;
        }

        function rectangleSelect(x, y, width, height) {
             var rows = (gridView.flow == GridView.FlowLeftToRight);
             var axis = rows ? gridView.width : gridView.height;
             var step = rows ? cellWidth : cellHeight;
             var perStripe = Math.floor(axis / step);
             var stripes = Math.ceil(gridView.count / perStripe);
             var cWidth = gridView.cellWidth - (2 * Meui.Units.smallSpacing);
             var cHeight = gridView.cellHeight - (2 * Meui.Units.smallSpacing);
             var midWidth = gridView.cellWidth / 2;
             var midHeight = gridView.cellHeight / 2;
             var indices = [];

             for (var s = 0; s < stripes; s++) {
                 for (var i = 0; i < perStripe; i++) {
                     var index = (s * perStripe) + i;

                     if (index >= gridView.count) {
                         break;
                     }

                     if (positioner.isBlank(index)) {
                         continue;
                     }

                     var itemX = ((rows ? i : s) * gridView.cellWidth);
                     var itemY = ((rows ? s : i) * gridView.cellHeight);

                     if (gridView.effectiveLayoutDirection == Qt.RightToLeft) {
                         itemX -= (rows ? gridView.contentX : gridView.originX);
                         itemX += cWidth;
                         itemX = (rows ? gridView.width : gridView.contentItem.width) - itemX;
                     }

                     // Check if the rubberband intersects this cell first to avoid doing more
                     // expensive work.
                     if (control.rubberBand.intersects(Qt.rect(itemX + Meui.Units.smallSpacing, itemY + Meui.Units.smallSpacing,
                         cWidth, cHeight))) {
                         var item = gridView.contentItem.childAt(itemX + midWidth, itemY + midHeight);

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

             gridView.cachedRectangleSelection = indices;
         }
    }

    FM.FolderModel {
        id: dir

        sortDirsFirst: true
        parseDesktopFiles: true
        viewAdapter: viewAdapter
        url: dir.desktopPath()
        previews: true
        previewPlugins: []

        onListingCompleted: {
            if (!gridView.model) {
                gridView.model = positioner;
                gridView.currentIndex = -1
            }
        }

        onMove: {
            var rows = (gridView.flow == GridView.FlowLeftToRight);
            var axis = rows ? gridView.width : gridView.height;
            var step = rows ? cellWidth : cellHeight;
            var perStripe = Math.floor(axis / step);
            var dropPos = mapToItem(gridView.contentItem, x, y);
            var leftEdge = Math.min(gridView.contentX, gridView.originX);

            var moves = []
            var itemX = -1;
            var itemY = -1;
            var col = -1;
            var row = -1;
            var from = -1;
            var to = -1;

            for (var i = 0; i < urls.length; i++) {
                from = positioner.indexForUrl(urls[i]);
                to = -1;

                if (from === -1) {
                    continue;
                }

                var offset = dir.dragCursorOffset(positioner.map(from));

                if (offset.x === -1) {
                    continue;
                }

                itemX = dropPos.x + offset.x + (listener.dragX % cellWidth) + (cellWidth / 2);
                itemY = dropPos.y + offset.y + (listener.dragY % cellHeight) + gridView.verticalDropHitscanOffset;

                if (gridView.effectiveLayoutDirection == Qt.RightToLeft) {
                    itemX -= (rows ? gridView.contentX : gridView.originX);
                    itemX = (rows ? gridView.width : gridView.contentItem.width) - itemX;
                }

                col = Math.floor(itemX / gridView.cellWidth);
                row = Math.floor(itemY / gridView.cellHeight);

                if ((rows ? col : row) < perStripe) {
                    to = ((rows ? row : col) * perStripe) + (rows ? col : row);

                    if (to < 0) {
                        return;
                    }
                }

                if (from !== to) {
                    moves.push(from);
                    moves.push(to);
                }
            }

            if (moves.length) {
                positioner.move(moves);
                gridView.forceLayout();
            }

            previouslySelectedItemIndex = -1;
            dir.clearSelection();
        }
    }

    FM.Positioner {
        id: positioner
        folderModel: dir
        enabled: true
        perStripe: Math.floor(((gridView.flow == GridView.FlowLeftToRight)
            ? gridView.width : gridView.height) / ((gridView.flow == GridView.FlowLeftToRight)
            ? gridView.cellWidth : gridView.cellHeight));
    }

    FM.ItemViewAdapter {
        id: viewAdapter

        adapterView: gridView
        adapterModel: positioner
        adapterIconSize: gridView.iconSize * 2
        adapterVisibleArea: Qt.rect(gridView.contentX, gridView.contentY, gridView.contentWidth, gridView.contentHeight)

        Component.onCompleted: {
            gridView.movementStarted.connect(viewAdapter.viewScrolled);
            dir.viewAdapter = viewAdapter;
        }
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

                // This easing curve has an elognated start, which works
                // better than a standard easing curve for the rubberband
                // animation, which fades out fast and is generally of a
                // small area.
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

    Component {
        id: editorComponent

        TextArea {
            id: editor

            visible: false

            wrapMode: TextEdit.Wrap

            textMargin: 0

            horizontalAlignment: TextEdit.AlignHCenter

            property Item targetItem: null

            onTargetItemChanged: {
                if (targetItem != null) {
                    var xy = getXY();
                    x = xy[0];
                    y = xy[1];
                    width = getWidth();
                    height = getInitHeight();
                    text = targetItem.label.text;
                    adjustSize();
                    editor.select(0, dir.fileExtensionBoundary(positioner.map(targetItem.index)));
                    if(isPopup) {
                        flickableItem.contentX = Math.max(flickableItem.contentWidth - contentItem.width, 0);
                    } else {
                        flickableItem.contentY = Math.max(flickableItem.contentHeight - contentItem.height, 0);
                    }
                    visible = true;
                } else {
                    x: 0
                    y: 0
                    visible = false;
                }
            }

            onVisibleChanged: {
                if (visible) {
                    focus = true;
                } else {
                    scrollArea.focus = true;
                }
            }

            Keys.onPressed: {
                switch(event.key) {
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    commit();
                    break;
                case Qt.Key_Escape:
                    if (targetItem) {
                        targetItem = null;
                        event.accepted = true;
                    }
                    break;
                case Qt.Key_Home:
                    if (event.modifiers & Qt.ShiftModifier) {
                        editor.select(0, cursorPosition);
                    } else {
                        editor.select(0, 0);
                    }
                    event.accepted = true;
                    break;
                case Qt.Key_End:
                    if (event.modifiers & Qt.ShiftModifier) {
                        editor.select(cursorPosition, text.length);
                    } else {
                        editor.select(text.length, text.length);
                    }
                    event.accepted = true;
                    break;
                default:
                    adjustSize();
                    break;
                }
            }

            Keys.onReleased: {
                adjustSize();
            }

            function getXY() {
                var pos = control.mapFromItem(targetItem, targetItem.labelArea.x, targetItem.labelArea.y);
                var _x, _y;

                _x = targetItem.x + Math.abs(Math.min(gridView.contentX, gridView.originX));
                _x += __style.padding.left;
                _x += scrollArea.viewport.x;
                if (verticalScrollBarPolicy == Qt.ScrollBarAlwaysOn
                    && gridView.effectiveLayoutDirection == Qt.RightToLeft) {
                    _x -= __verticalScrollBar.parent.verticalScrollbarOffset;
                }
                _y = pos.y + PlasmaCore.Units.smallSpacing - __style.padding.top;

                return([ _x, _y ]);
            }

            function getWidth(addWidthVerticalScroller) {
                 return targetItem.label.parent.width - PlasmaCore.Units.smallSpacing;
            }

            function getHeight(addWidthHoriozontalScroller, init) {
                var _height;
                if (init) {
                    _height = targetItem.labelArea.height + __style.padding.top + __style.padding.bottom;
                } else {
                    var realHeight = contentHeight + __style.padding.top + __style.padding.bottom;
                    _height = realHeight;
                }
                return(_height + (addWidthHoriozontalScroller ? __horizontalScrollBar.parent.horizontalScrollbarOffset : 0));
            }

            function getInitHeight() {
                return(getHeight(false, true));
            }

            function adjustSize() {
                if (isPopup) {
                    if (contentWidth + __style.padding.left + __style.padding.right > width) {
                        visible = true;
                        horizontalScrollBarPolicy = Qt.ScrollBarAlwaysOn;
                        height = getHeight(true);
                    } else {
                        horizontalScrollBarPolicy = Qt.ScrollBarAlwaysOff;
                        height = getHeight();
                    }
                } else {
                    height = getHeight();
                    if(contentHeight + __style.padding.top + __style.padding.bottom > height) {
                        visible = true;
                        verticalScrollBarPolicy = Qt.ScrollBarAlwaysOn;
                        width = getWidth(true);
                    } else {
                        verticalScrollBarPolicy = Qt.ScrollBarAlwaysOff;
                        width = getWidth();
                    }
                }

                var xy = getXY();
                x = xy[0];
                y = xy[1];
            }

            function commit() {
                if (targetItem) {
                    dir.rename(positioner.map(targetItem.index), text);
                    targetItem = null;
                }
            }
        }

//        Component.onCompleted: {
//            dir.requestRename.connect(rename);
//        }
    }
}
