import QtQuick 2.12
import QtQuick.Controls 2.12
import FishUI 1.0 as FishUI

FishUI.TextArea {
    id: editor

    required property Item view
    required property Item targetItem
    required property string originalName
    property int selectionEnd: originalName.length
    property bool centered: false
    property bool finishing: false
    property string errorMessage: ""

    signal submitted(string name)
    signal finished()

    z: 999
    visible: targetItem !== null
    font: targetItem ? targetItem.labelArea.font : FishUI.Theme.defaultFont
    color: FishUI.Theme.textColor
    selectionColor: FishUI.Theme.highlightColor
    selectedTextColor: "white"
    selectByMouse: true
    persistentSelection: true
    wrapMode: centered ? TextEdit.Wrap : TextEdit.NoWrap
    horizontalAlignment: centered ? TextEdit.AlignHCenter : TextEdit.AlignLeft
    verticalAlignment: TextEdit.AlignVCenter
    padding: 4
    readonly property real labelCenterX: targetItem
        ? targetItem.labelArea.mapToItem(view, targetItem.labelArea.width / 2, 0).x : 0
    // Shrink at the edges without moving the editor away from the icon center.
    readonly property real centeredWidth: targetItem ? Math.max(0, Math.min(
        targetItem.labelArea.width + leftPadding + rightPadding,
        2 * Math.min(labelCenterX - 4, view.width - labelCenterX - 4))) : 0
    width: targetItem ? (centered ? centeredWidth
        : Math.max(0, Math.min(view.width - 8, Math.max(120, targetItem.labelArea.width + 8)))) : 0
    height: Math.max(fontMetrics.height + 8, contentHeight + topPadding + bottomPadding)
    x: targetItem ? (centered ? labelCenterX - width / 2
        : Math.max(4, Math.min(view.width - width - 4,
            targetItem.labelArea.mapToItem(view, 0, 0).x - leftPadding))) : 0
    y: targetItem ? targetItem.labelArea.mapToItem(view, 0, 0).y - topPadding : 0

    Accessible.name: qsTr("Rename")
    Accessible.description: errorMessage || qsTr("Return to save, Escape to cancel")

    FontMetrics {
        id: fontMetrics
        font: editor.font
    }

    background: Rectangle {
        radius: 4
        color: FishUI.Theme.backgroundColor
        border.width: 1
        border.color: editor.errorMessage ? "#d94b4b" : FishUI.Theme.highlightColor

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: 6
            color: "transparent"
            border.width: 2
            border.color: parent.border.color
            opacity: 0.25
        }
    }

    FishUI.ToolTip {
        visible: editor.errorMessage.length > 0
        text: editor.errorMessage
        delay: 0
        timeout: -1
    }

    function begin() {
        text = originalName
        targetItem.labelArea.visible = false
        forceActiveFocus()
        select(0, Math.max(0, Math.min(selectionEnd, text.length)))
    }

    function commit() {
        if (finishing || !targetItem)
            return true

        if (!text.trim().length)
            errorMessage = qsTr("Enter a name.")
        else if (text === "." || text === "..")
            errorMessage = qsTr("Choose a name other than . or ..")
        else if (/[\/\r\n\u0000]/.test(text))
            errorMessage = qsTr("Names cannot contain / or line breaks.")
        else
            errorMessage = ""

        if (errorMessage) {
            forceActiveFocus()
            return false
        }

        finishing = true
        if (text !== originalName)
            submitted(text)
        finish()
        return true
    }

    function cancel() {
        if (finishing)
            return
        finishing = true
        finish()
    }

    function finish() {
        if (targetItem)
            targetItem.labelArea.visible = true
        targetItem = null
        view.forceActiveFocus()
        finished()
    }

    onTextChanged: errorMessage = ""

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (!inputMethodComposing)
                commit()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            cancel()
            event.accepted = true
        } else if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) {
            commit()
            event.accepted = true
        }
    }
}
