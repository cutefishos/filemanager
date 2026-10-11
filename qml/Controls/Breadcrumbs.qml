import QtQuick
import FishUI 1.0 as FishUI

// The crumbs of a PathBarModel. When they do not fit, the names between the
// root and the current folder shorten first, then those folders fold into a
// "…" crumb, then the root does too; the current folder is elided only once
// there is nothing left to fold.
Item {
    id: control

    property alias model: _repeater.model
    // Right-clicking a crumb offers opening it in a new tab or window.
    property bool openActionsEnabled: false

    readonly property color ancestorColor: Qt.rgba(FishUI.Theme.textColor.r,
                                                  FishUI.Theme.textColor.g,
                                                  FishUI.Theme.textColor.b, 0.6)

    signal itemClicked(string path)
    signal openInNewTab(string path)
    signal openInNewWindow(string path)

    // With the crumb's own half-largeSpacing, text starts where the editor's does.
    readonly property real sideMargin: FishUI.Units.smallSpacing
    readonly property real crumbPadding: FishUI.Units.largeSpacing
    readonly property real separatorWidth: 10 + FishUI.Units.smallSpacing
    // One long folder name must not push every other ancestor into the fold.
    readonly property real ancestorMaxTextWidth: FishUI.Units.gridUnit * 8
    // How short a name between the root and the current folder gets before it folds.
    readonly property real ancestorMinTextWidth: FishUI.Units.gridUnit * 4
    readonly property real currentMinTextWidth: FishUI.Units.gridUnit * 2
    // The press highlight hugs the text; the crumb keeps the bar's height to hit.
    readonly property real highlightHeight: _foldLabel.implicitHeight + FishUI.Units.smallSpacing

    // Model rows foldFirst..foldLast sit in the "…" crumb; -1 when none do.
    property int foldFirst: -1
    property int foldLast: -1
    property var foldedItems: []
    property real foldX: 0
    property var positions: []
    property real currentTextWidth: 0
    // What the names between the root and the current folder are shortened to.
    property real middleTextWidth: ancestorMaxTextWidth

    clip: true

    // Laid out at once rather than on a later tick, so a new folder's crumbs
    // never show a frame unplaced.
    onWidthChanged: relayout()

    function relayout() {
        var n = _repeater.count
        var natural = []

        for (var i = 0; i < n; ++i) {
            var item = _repeater.itemAt(i)
            if (!item)
                return
            natural.push(item.naturalTextWidth)
        }

        function crumbWidth(i, middle) {
            var text = i === n - 1 ? natural[i]
                                   : Math.min(natural[i], i === 0 ? ancestorMaxTextWidth : middle)
            return text + crumbPadding
        }

        var foldWidth = _foldLabel.implicitWidth + crumbPadding

        function totalWidth(fold, middle) {
            var width = 0
            var count = 0
            if (fold[0] >= 0) {
                width += foldWidth
                ++count
            }
            for (var i = 0; i < n; ++i) {
                if (i >= fold[0] && i <= fold[1])
                    continue
                width += crumbWidth(i, middle)
                ++count
            }
            return width + separatorWidth * Math.max(0, count - 1)
        }

        // Folding order: the middle from the root side, then the root with it,
        // then everything but the current folder.
        var candidates = [[-1, -1]]
        for (var last = 1; last <= n - 3; ++last)
            candidates.push([1, last])
        if (n >= 3)
            candidates.push([0, n - 3])
        if (n >= 2)
            candidates.push([0, n - 2])

        var available = width - sideMargin * 2
        var fold = candidates[candidates.length - 1]
        var middle = ancestorMaxTextWidth
        for (var c = 0; c < candidates.length; ++c) {
            if (totalWidth(candidates[c], ancestorMaxTextWidth) <= available) {
                fold = candidates[c]
                break
            }
            if (totalWidth(candidates[c], ancestorMinTextWidth) <= available) {
                // The longest names give way first: the widest cap that fits.
                var low = ancestorMinTextWidth
                var high = ancestorMaxTextWidth
                while (high - low > 0.5) {
                    var mid = (low + high) / 2
                    if (totalWidth(candidates[c], mid) <= available)
                        low = mid
                    else
                        high = mid
                }
                fold = candidates[c]
                middle = Math.floor(low)
                break
            }
        }

        var others = n > 0 ? totalWidth(fold, middle) - crumbWidth(n - 1, middle) : 0
        currentTextWidth = n > 0 ? Math.min(natural[n - 1],
                                            Math.max(currentMinTextWidth, available - others - crumbPadding))
                                 : 0

        var folded = []
        var xs = []
        var x = sideMargin
        for (var j = 0; j < n; ++j) {
            if (j === fold[0]) {
                foldX = x
                x += foldWidth + separatorWidth
            }
            if (j >= fold[0] && j <= fold[1]) {
                var foldedItem = _repeater.itemAt(j)
                folded.push({ name: foldedItem.name, path: foldedItem.path })
                xs.push(0)
                continue
            }
            xs.push(x)
            x += (j === n - 1 ? currentTextWidth + crumbPadding : crumbWidth(j, middle)) + separatorWidth
        }

        foldFirst = fold[0]
        foldLast = fold[1]
        middleTextWidth = middle
        foldedItems = folded
        positions = xs
    }

    Repeater {
        id: _repeater

        onItemAdded: control.relayout()
        onItemRemoved: control.relayout()

        delegate: MouseArea {
            id: _item

            required property int index
            required property var model

            readonly property string name: model.name
            readonly property string path: model.path
            readonly property real naturalTextWidth: _name.implicitWidth
            readonly property bool selected: index === _repeater.count - 1
            readonly property bool folded: index >= control.foldFirst && index <= control.foldLast

            property bool dragged: false
            property real pressX: 0
            property real pressY: 0

            x: control.positions[index] ?? 0
            y: 2
            height: control.height - 4
            width: _name.width + control.crumbPadding
            // A delegate on its way out of a model reset has index -1.
            visible: index >= 0 && !folded && control.positions.length === _repeater.count
            hoverEnabled: true
            acceptedButtons: control.openActionsEnabled ? Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                                                        : Qt.LeftButton

            onNaturalTextWidthChanged: control.relayout()

            onPressed: function(mouse) {
                dragged = false
                pressX = mouse.x
                pressY = mouse.y
            }
            onPositionChanged: function(mouse) {
                if (pressed && (Math.abs(mouse.x - pressX) > 4 || Math.abs(mouse.y - pressY) > 4))
                    dragged = true
            }
            onCanceled: dragged = true
            onClicked: function(mouse) {
                if (dragged)
                    return
                if (mouse.button === Qt.RightButton)
                    _crumbMenu.popup()
                else if (mouse.button === Qt.MiddleButton)
                    control.openInNewTab(path)
                else
                    control.itemClicked(path)
            }

            FishUI.DesktopMenu {
                id: _crumbMenu
                pointerGrabEnabled: true

                FishUI.MenuItem {
                    text: qsTr("Open In New Tab")
                    onTriggered: control.openInNewTab(_item.path)
                }
                FishUI.MenuItem {
                    text: qsTr("Open In New Window")
                    onTriggered: control.openInNewWindow(_item.path)
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: control.highlightHeight
                radius: FishUI.Theme.smallRadius
                color: _item.pressed ? Qt.rgba(FishUI.Theme.textColor.r,
                                               FishUI.Theme.textColor.g,
                                               FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1)
                                     : "transparent"
                smooth: true
            }

            FishUI.Label {
                id: _name
                anchors.centerIn: parent
                width: Math.min(implicitWidth, _item.selected ? control.currentTextWidth
                                             : _item.index === 0 ? control.ancestorMaxTextWidth
                                                                 : control.middleTextWidth)
                text: _item.name
                textFormat: Text.PlainText
                // The end of a name, often a version or a suffix, tells folders apart.
                elide: _item.selected ? Text.ElideMiddle : Text.ElideRight
                color: _item.selected ? FishUI.Theme.textColor : control.ancestorColor
            }

            FishUI.Icon {
                anchors.left: parent.right
                anchors.leftMargin: FishUI.Units.smallSpacing / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 10
                visible: !_item.selected
                name: "chevron-right"
                color: control.ancestorColor
            }

            FishUI.ToolTip {
                visible: _item.containsMouse && !_item.pressed && _name.truncated
                text: _item.name
            }
        }
    }

    MouseArea {
        id: _fold

        property bool dragged: false
        property real pressX: 0
        property real pressY: 0

        x: control.foldX
        y: 2
        height: control.height - 4
        width: _foldLabel.implicitWidth + control.crumbPadding
        visible: control.foldFirst >= 0 && control.positions.length === _repeater.count
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPressed: function(mouse) {
            dragged = false
            pressX = mouse.x
            pressY = mouse.y
        }
        onPositionChanged: function(mouse) {
            if (pressed && (Math.abs(mouse.x - pressX) > 4 || Math.abs(mouse.y - pressY) > 4))
                dragged = true
        }
        onCanceled: dragged = true
        onClicked: {
            if (!dragged)
                _foldMenu.popup()
        }

        FishUI.DesktopMenu {
            id: _foldMenu
            pointerGrabEnabled: true

            Repeater {
                model: control.foldedItems

                FishUI.MenuItem {
                    required property var modelData

                    text: modelData.name
                    onTriggered: control.itemClicked(modelData.path)
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: control.highlightHeight
            radius: FishUI.Theme.smallRadius
            color: _fold.pressed || _foldMenu.visible ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                FishUI.Theme.textColor.g,
                                                                FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1)
                                                      : "transparent"
            smooth: true
        }

        FishUI.Label {
            id: _foldLabel
            anchors.centerIn: parent
            text: "…"
            color: control.ancestorColor
        }

        FishUI.Icon {
            anchors.left: parent.right
            anchors.leftMargin: FishUI.Units.smallSpacing / 2
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            name: "chevron-right"
            color: control.ancestorColor
        }
    }
}
