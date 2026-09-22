import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import FishUI 1.0 as FishUI

FishUI.Window {
    id: control
    title: heading
    flags: Qt.Dialog | Qt.FramelessWindowHint
    visible: false
    minimizeButtonVisible: false
    windowButtonsTopMargin: 8
    windowButtonsRightMargin: 8
    header.height: 36
    background.color: FishUI.Theme.secondBackgroundColor

    readonly property int dialogWidth: Math.max(480, actions.implicitWidth + 40)
    readonly property int dialogHeight: content.implicitHeight + header.height + 36
    width: dialogWidth
    height: dialogHeight
    minimumWidth: dialogWidth
    maximumWidth: dialogWidth
    minimumHeight: dialogHeight
    maximumHeight: dialogHeight

    property bool finished: false
    property bool conflict: false
    property bool canceling: false
    property string errorMessage: ""
    property string conflictSource: ""
    property string conflictPath: ""
    property string operation: ""
    property string sourceName: ""
    property string destinationName: ""
    property int sourceCount: 0
    property var info: ioTask ? ioTask.progressInfo : null

    DragHandler {
        parent: control.contentItem
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        grabPermissions: PointerHandler.CanTakeOverFromItems
                         | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                         | PointerHandler.ApprovesTakeOverByAnything
        onActiveChanged: if (active) control.helper.startSystemMove(control)
    }

    readonly property string heading: operationHeading()
    readonly property string currentName: info && info.currentPath.length
                                          ? fileName(info.currentPath) : sourceName
    readonly property bool usesBytes: info && info.totalBytes > 0
                                      && ["delete", "emptyTrash", "rename", "link"].indexOf(operation) < 0
    readonly property real completedUnits: !info ? 0 : usesBytes ? info.processedBytes : info.processedItems
    readonly property real totalUnits: !info ? 0 : usesBytes ? info.totalBytes : info.totalItems
    readonly property real fraction: totalUnits > 0 ? Math.min(1, completedUnits / totalUnits) : 0
    readonly property string summary: progressSummary()

    Component.onCompleted: {
        operation = ioTask.operationName
        sourceName = ioTask.sourceName
        sourceCount = ioTask.sourceCount
        destinationName = ioTask.destinationName
        if (ioTask.finished) {
            delay.stop()
            finished = true
            if (ioTask.error() && !ioTask.wasCanceled()) {
                errorMessage = ioTask.errorString()
                show()
            }
        } else if (ioTask.waitingForConflict) {
            askConflict(ioTask.conflictSource, ioTask.conflictPath)
        }
    }
    onVisibilityChanged: function(visibility) {
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            showNormal()
    }
    function toggleMaximized() {}

    function fileName(path) {
        return path.substring(path.lastIndexOf("/") + 1)
    }
    function operationHeading() {
        switch (operation) {
        case "copy": return sourceCount === 1
                    ? qsTr("Copying “%1” to “%2”").arg(sourceName).arg(destinationName)
                    : qsTr("Copying %1 items to “%2”").arg(sourceCount).arg(destinationName)
        case "move": return sourceCount === 1
                    ? qsTr("Moving “%1” to “%2”").arg(sourceName).arg(destinationName)
                    : qsTr("Moving %1 items to “%2”").arg(sourceCount).arg(destinationName)
        case "trash": return sourceCount === 1
                     ? qsTr("Moving “%1” to Trash").arg(sourceName)
                     : qsTr("Moving %1 items to Trash").arg(sourceCount)
        case "restore": return sourceCount === 1
                       ? qsTr("Restoring “%1”").arg(sourceName)
                       : qsTr("Restoring %1 items").arg(sourceCount)
        case "delete": return sourceCount === 1
                      ? qsTr("Deleting “%1”").arg(sourceName)
                      : qsTr("Deleting %1 items").arg(sourceCount)
        case "emptyTrash": return qsTr("Emptying Trash")
        case "rename": return qsTr("Renaming “%1” to “%2”").arg(sourceName).arg(destinationName)
        case "mkdir": return qsTr("Creating “%1”").arg(destinationName)
        case "link": return qsTr("Creating links in “%1”").arg(destinationName)
        case "compress": return qsTr("Compressing to “%1”").arg(destinationName)
        case "extract": return qsTr("Extracting to “%1”").arg(destinationName)
        default: return qsTr("Preparing…")
        }
    }
    function timeRemaining() {
        if (!info || info.elapsedMs < 1000 || fraction <= 0 || fraction >= 1)
            return ""
        const seconds = Math.max(1, Math.round(info.elapsedMs / 1000 * (1 - fraction) / fraction))
        if (seconds < 60)
            return seconds === 1 ? qsTr("About a second remaining") : qsTr("About %1 seconds remaining").arg(seconds)
        const minutes = Math.ceil(seconds / 60)
        return minutes === 1 ? qsTr("About a minute remaining") : qsTr("About %1 minutes remaining").arg(minutes)
    }
    function progressSummary() {
        if (canceling)
            return qsTr("Stopping…")
        if (!info || info.preparing)
            return qsTr("Preparing…")
        if (conflict)
            return qsTr("Waiting for your choice")
        if (totalUnits > 0 && fraction >= 1 && !finished)
            return qsTr("Finishing…")
        if (!info.totalsKnown)
            return currentName.length ? qsTr("Working on “%1”…").arg(currentName) : qsTr("Working…")
        const amount = usesBytes && ioTask
                     ? qsTr("%1 of %2").arg(ioTask.formattedSize(Math.min(info.processedBytes, info.totalBytes)))
                                       .arg(ioTask.formattedSize(info.totalBytes))
                     : qsTr("%1 of %2 items").arg(Math.min(info.processedItems, info.totalItems)).arg(info.totalItems)
        const remaining = timeRemaining()
        return remaining.length ? amount + " — " + remaining : amount
    }
    function cancelTask() {
        if (finished || canceling || !ioTask)
            return
        canceling = true
        conflict = false
        if (conflictDialog.visible)
            conflictDialog.close()
        ioTask.cancel()
    }
    // The conflict dialog stands in for the progress window: the latter is
    // hidden while the question is open and comes back through the usual
    // show delay once the task is running again.
    function askConflict(source, destination) {
        conflictSource = source
        conflictPath = destination
        conflict = true
        delay.stop()
        if (visible)
            hide()
        conflictDialog.sourcePath = source
        conflictDialog.destinationPath = destination
        // A raw IO task (tests) has no fileDetails; the dialog then falls
        // back to the file name alone.
        const details = typeof ioTask.fileDetails === "function"
        conflictDialog.existing = details ? ioTask.fileDetails(destination) : {}
        conflictDialog.incoming = details ? ioTask.fileDetails(source) : {}
        conflictDialog.open()
    }
    function resolve(policy, applyToAll) {
        conflict = false
        if (policy < 0)
            cancelTask()
        else
            ioTask.resolveConflict(policy, applyToAll === true)
        if (!finished)
            delay.restart()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: control.close()
    }
    Timer {
        id: delay
        objectName: "ioShowDelay"
        interval: control.operation === "emptyTrash" ? 300 : 750
        running: true
        onTriggered: if (!control.finished && !control.conflict) control.show()
    }
    Connections {
        target: ioTask
        function onConflictRequested(source, destination) {
            control.askConflict(source, destination)
        }
        // A remote task may announce the conflict through its snapshot
        // before, or instead of, the signal.
        function onProgressInfoChanged() {
            if (ioTask.waitingForConflict && !control.conflict && !control.finished)
                control.askConflict(ioTask.conflictSource, ioTask.conflictPath)
        }
        function onResult(task) {
            delay.stop()
            control.finished = true
            control.conflict = false
            if (conflictDialog.visible)
                conflictDialog.close()
            if (task.error() && !task.wasCanceled()) {
                control.errorMessage = task.errorString()
                control.show()
            } else {
                control.close()
            }
        }
    }

    ConflictDialog {
        id: conflictDialog
        operation: control.operation
        sourceCount: control.sourceCount
        onResolved: function(policy, applyToAll) {
            if (control.conflict)
                control.resolve(policy, applyToAll)
        }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            FishUI.IconItem {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                Layout.alignment: Qt.AlignTop
                source: control.operation === "trash" || control.operation === "emptyTrash"
                        ? "user-trash" : "folder"
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                FishUI.Label {
                    objectName: "ioOperationHeading"
                    Layout.fillWidth: true
                    text: control.heading
                    textFormat: Text.PlainText
                    font: Qt.font({ family: FishUI.Theme.fontFamily, pixelSize: 13, weight: Font.DemiBold })
                    color: FishUI.Theme.textColor
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideMiddle
                }
                FishUI.Label {
                    Layout.fillWidth: true
                    text: control.errorMessage.length ? control.errorMessage
                          : control.conflict ? qsTr("“%1” already exists in this location.").arg(control.fileName(control.conflictPath))
                          : control.currentName
                    textFormat: Text.PlainText
                    font: Qt.font({ family: FishUI.Theme.fontFamily, pixelSize: 12 })
                    color: FishUI.Theme.disabledTextColor
                    wrapMode: control.finished || control.conflict ? Text.Wrap : Text.NoWrap
                    maximumLineCount: 4
                    elide: Text.ElideMiddle
                    visible: text.length > 0
                }
                FishUI.ProgressBar {
                    id: progressBar
                    objectName: "ioProgressBar"
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    visible: !control.finished
                    from: 0
                    to: 1
                    value: Math.min(0.99, control.fraction)
                    indeterminate: !control.canceling && !control.conflict
                                   && (!control.info || control.info.preparing || !control.info.totalsKnown)
                }
                FishUI.Label {
                    objectName: "ioProgressSummary"
                    Layout.fillWidth: true
                    visible: !control.finished
                    text: control.summary
                    textFormat: Text.PlainText
                    font: Qt.font({ family: FishUI.Theme.fontFamily, pixelSize: 11 })
                    color: FishUI.Theme.disabledTextColor
                    wrapMode: Text.Wrap
                }
            }
        }
        RowLayout {
            id: actions
            Layout.alignment: Qt.AlignRight
            spacing: 8
            FishUI.ConfirmDialogButton {
                objectName: "ioCancelButton"
                text: control.finished ? qsTr("Close") : qsTr("Cancel")
                enabled: control.finished || !control.canceling
                onClicked: control.finished ? control.close() : control.cancelTask()
            }
        }
    }
}
