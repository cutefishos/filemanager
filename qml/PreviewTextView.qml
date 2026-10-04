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

    onFileChanged: {
        textEdit.deselect()
        flick.contentY = 0
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
            text: viewer.file.text || ""
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
            visible: !!viewer.file.truncated
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
