// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI 1.0 as FishUI

// A text file on an opaque page, wrapped to its width and selectable.
Rectangle {
    id: viewer
    objectName: "previewTextView"
    color: FishUI.Theme.secondBackgroundColor
    clip: true

    // The file description, with `text`, `truncated` and `breaks`: the line
    // breaks inserted into long paragraphs, which copying leaves out.
    property var file: ({})
    property alias font: textEdit.font
    readonly property int margin: 20

    // TextEdit lays out all it is given at once (80 ms for 256 KB on the
    // daemon's GUI thread), so the text arrives a slice per frame. The first is
    // over 10,000 characters: only then, checked when the text is set, does
    // TextEdit build nodes for the paragraphs in view alone.
    readonly property int firstSlice: 10240
    readonly property int slice: 8192
    property string fullText: ""
    property int loaded: 0
    readonly property bool complete: loaded >= fullText.length

    signal copyRequested(string text)

    function originalText(start, end) {
        const text = textEdit.text
        let result = ""
        let position = start
        for (const at of file.breaks || []) {
            if (at < start)
                continue
            if (at >= end)
                break
            result += text.slice(position, at)
            position = at + 1
        }
        return result + text.slice(position, end)
    }

    // Ends a slice after a whole line; long paragraphs are already split.
    function sliceEnd(from, size) {
        if (from + size >= fullText.length)
            return fullText.length
        const end = fullText.indexOf("\n", from + size)
        return end < 0 ? fullText.length : end + 1
    }

    function load() {
        const text = file.text || ""
        if (text === fullText)
            return
        fullText = text
        loaded = sliceEnd(0, firstSlice)
        textEdit.text = text.slice(0, loaded)
    }

    onFileChanged: {
        textEdit.deselect()
        flick.contentY = 0
        load()
    }

    Component.onCompleted: load()

    Connections {
        target: viewer.Window.window
        enabled: !viewer.complete

        function onAfterAnimating() {
            const end = viewer.sliceEnd(viewer.loaded, viewer.slice)
            textEdit.insert(textEdit.length, viewer.fullText.slice(viewer.loaded, end))
            viewer.loaded = end
        }
    }

    FontMetrics {
        id: metrics
        font: textEdit.font
    }

    // Moved by the wheel handler only, so a drag selects text.
    Flickable {
        id: flick
        anchors.fill: parent
        interactive: false
        contentWidth: width
        contentHeight: textEdit.height + (notice.visible ? notice.height + viewer.margin : 0)

        FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

        TextEdit {
            id: textEdit
            objectName: "previewText"
            width: flick.width
            padding: viewer.margin
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            tabStopDistance: metrics.advanceWidth(" ") * 8
            readOnly: true
            selectByMouse: true
            persistentSelection: true
            color: FishUI.Theme.textColor
            selectionColor: FishUI.Theme.highlightColor
            selectedTextColor: FishUI.Theme.highlightedTextColor
            renderType: FishUI.Theme.renderType

            Keys.onPressed: function(event) {
                if (event.matches(StandardKey.Copy) && selectedText.length > 0) {
                    viewer.copyRequested(viewer.originalText(selectionStart, selectionEnd))
                    event.accepted = true
                }
            }
        }

        FishUI.Label {
            id: notice
            visible: !!viewer.file.truncated && viewer.complete
            anchors.top: textEdit.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: viewer.margin
            anchors.rightMargin: viewer.margin
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("The rest of this file is not shown.")
            color: FishUI.Theme.disabledTextColor
        }
    }

    FishUI.WheelHandler {
        target: flick
    }
}
