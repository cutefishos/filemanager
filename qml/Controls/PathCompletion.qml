import QtQuick
import QtQuick.Templates as T
import Cutefish.FileManager 1.0
import FishUI 1.0 as FishUI

// The folders completing what is typed into a path editor, listed under it.
// The keyboard stays with the editor, which passes keys on through handleKey().
T.Popup {
    id: control

    required property Item editor
    readonly property alias count: _model.count

    // Up to here the list follows the editor; from here the keys decide.
    property bool edited: false
    // Escape hides the list until the next edit.
    property bool dismissed: false
    property int currentIndex: -1

    readonly property int maximumRows: 8
    readonly property int rowHeight: 30

    signal activated(string path)

    parent: control.editor
    x: 0
    y: control.editor.height + FishUI.Units.smallSpacing / 2
    width: control.editor.width
    margins: FishUI.Units.smallSpacing
    padding: 4
    implicitHeight: Math.min(_model.count, control.maximumRows) * (control.rowHeight + _list.spacing)
                    - _list.spacing + topPadding + bottomPadding

    closePolicy: T.Popup.NoAutoClose
    visible: control.editor.visible && control.editor.activeFocus
             && control.edited && !control.dismissed && _model.count > 0

    background: FishUI.RoundedRectangle {
        color: FishUI.Theme.secondBackgroundColor
        radius: FishUI.Theme.mediumRadius

        layer.enabled: true
        layer.effect: FishUI.DropShadow {
            transparentBorder: true
            radius: 12
            samples: 16
            horizontalOffset: 0
            verticalOffset: 2
            color: Qt.rgba(0, 0, 0, 0.18)
        }
    }

    contentItem: ListView {
        id: _list

        clip: true
        spacing: 1
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: control.currentIndex
        highlightFollowsCurrentItem: false
        // No rows are built for a list nobody sees, nor ahead of scrolling:
        // each keystroke resets the model under rows still being built.
        model: control.visible ? _model : null
        cacheBuffer: 0

        delegate: FishUI.MenuItem {
            required property int index
            required property string name
            required property string path

            width: ListView.view.width
            text: name
            icon.name: "folder"
            // A press must leave the editor focused, or it closes under the click.
            focusPolicy: Qt.NoFocus
            // The keys and the pointer share one lit row.
            pointerTracking: true
            highlighted: index === control.currentIndex

            onTriggered: control.activated(path)
        }

        // Only a pointer that moves lights a row: hover also follows rows
        // refiltered under a resting pointer, and Return would then take one.
        HoverHandler {
            property bool tracking: false
            property point lastPosition

            onHoveredChanged: tracking = false
            onPointChanged: {
                var position = point.scenePosition
                var moved = tracking && (position.x !== lastPosition.x || position.y !== lastPosition.y)
                tracking = true
                lastPosition = position
                if (!moved)
                    return

                var index = _list.indexAt(point.position.x + _list.contentX, point.position.y + _list.contentY)
                if (index >= 0)
                    control.currentIndex = index
            }
        }
    }

    PathCompletionModel {
        id: _model
        // Empty while not editing, so the next edit lists the folders afresh.
        text: control.edited ? control.editor.text : ""
    }

    Connections {
        target: control.editor

        function onTextChanged() {
            control.currentIndex = -1
        }
        function onTextEdited() {
            control.edited = true
            control.dismissed = false
        }
    }

    function containsScenePosition(position) {
        var item = control.background
        return control.visible && item.contains(item.mapFromItem(null, position.x, position.y))
    }

    function reset() {
        control.edited = false
        control.dismissed = false
        control.currentIndex = -1
    }

    function move(step) {
        var index = control.currentIndex < 0 ? (step > 0 ? 0 : _list.count - 1)
                                             : control.currentIndex + step
        control.currentIndex = Math.max(0, Math.min(_list.count - 1, index))
        _list.positionViewAtIndex(control.currentIndex, ListView.Contain)
    }

    // Tab takes the lit folder, or as much as all the folders share; a single
    // folder gets its slash so the next level lists at once.
    function complete() {
        var single = control.currentIndex >= 0 || _model.count === 1
        var text = control.currentIndex >= 0 ? _model.completion(control.currentIndex)
                                             : _model.commonCompletion
        if (single)
            text += "/"

        control.editor.text = text
        control.editor.cursorPosition = text.length
        control.edited = true
        control.dismissed = false
    }

    // Whether the key was for the list; the editor handles it otherwise.
    function handleKey(event) {
        if (!control.edited || _model.count === 0)
            return false

        switch (event.key) {
        case Qt.Key_Tab:
            control.complete()
            return true
        case Qt.Key_Down:
            if (control.dismissed) {
                control.dismissed = false
                return true
            }
            control.move(1)
            return true
        case Qt.Key_Up:
            if (!control.visible)
                return false
            control.move(-1)
            return true
        case Qt.Key_PageDown:
        case Qt.Key_PageUp:
            if (!control.visible)
                return false
            control.move(event.key === Qt.Key_PageDown ? control.maximumRows - 1 : -(control.maximumRows - 1))
            return true
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (!control.visible || control.currentIndex < 0)
                return false
            control.activated(_model.path(control.currentIndex))
            return true
        case Qt.Key_Escape:
            if (!control.visible)
                return false
            control.dismissed = true
            return true
        }

        return false
    }
}
