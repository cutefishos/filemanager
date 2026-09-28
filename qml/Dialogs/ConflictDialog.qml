import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import FishUI 1.0 as FishUI

FishUI.Window {
    id: control
    objectName: "conflictDialog"

    property string operation: ""
    property int sourceCount: 0
    property string sourcePath: ""
    property string destinationPath: ""
    property var existing: ({})
    property var incoming: ({})

    // policy follows Cutefish::IO::ConflictPolicy; -1 stops the whole task.
    signal resolved(int policy, bool applyToAll)

    title: qsTr("Replace Item")
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false
    minimizeButtonVisible: false
    contentTopMargin: 0
    windowButtonsTopMargin: 8
    windowButtonsRightMargin: 8
    header.height: 44
    background.color: FishUI.Theme.secondBackgroundColor
    // The background item reaches a grown window a frame or two after its buffer does.
    color: background.color

    // The window buttons sit inside the content area (contentTopMargin is
    // 0), so the text keeps 32px clear of the close button on the right.
    readonly property int padding: 20
    readonly property int dialogWidth: Math.max(480, actions.implicitWidth + padding * 2)
    readonly property int dialogHeight: layout.implicitHeight + padding * 2
    readonly property int textWidth: dialogWidth - padding * 2 - 64 - 16 - 32

    width: dialogWidth
    height: dialogHeight
    minimumWidth: dialogWidth
    maximumWidth: dialogWidth
    minimumHeight: dialogHeight
    maximumHeight: dialogHeight

    property bool _answered: false

    readonly property string itemName: existing && existing.name ? existing.name
                                       : destinationPath.substring(destinationPath.lastIndexOf("/") + 1)
    readonly property int ageComparison: compareModified()
    readonly property string heading: conflictHeading()
    readonly property string question: conflictQuestion()

    function compareModified() {
        if (!existing || !incoming || !existing.exists || !incoming.exists)
            return 0
        const a = new Date(existing.modified).getTime()
        const b = new Date(incoming.modified).getTime()
        if (isNaN(a) || isNaN(b) || Math.abs(a - b) < 1000)
            return 0
        return a < b ? -1 : 1
    }
    function conflictHeading() {
        switch (ageComparison) {
        case -1: return qsTr("An older item named “%1” already exists in this location.").arg(itemName)
        case 1: return qsTr("A newer item named “%1” already exists in this location.").arg(itemName)
        default: return qsTr("An item named “%1” already exists in this location.").arg(itemName)
        }
    }
    function conflictQuestion() {
        switch (operation) {
        case "move":
            return ageComparison < 0 ? qsTr("Do you want to replace it with the newer one you’re moving?")
                 : ageComparison > 0 ? qsTr("Do you want to replace it with the older one you’re moving?")
                                     : qsTr("Do you want to replace it with the one you’re moving?")
        case "link":
            return qsTr("Do you want to replace it with the link you’re creating?")
        case "restore":
            return ageComparison < 0 ? qsTr("Do you want to replace it with the newer one you’re restoring?")
                 : ageComparison > 0 ? qsTr("Do you want to replace it with the older one you’re restoring?")
                                     : qsTr("Do you want to replace it with the one you’re restoring?")
        default:
            return ageComparison < 0 ? qsTr("Do you want to replace it with the newer one you’re copying?")
                 : ageComparison > 0 ? qsTr("Do you want to replace it with the older one you’re copying?")
                                     : qsTr("Do you want to replace it with the one you’re copying?")
        }
    }
    function answer(policy) {
        _answered = true
        resolved(policy, applyToAll.checked)
        close()
    }
    function open() {
        _answered = false
        applyToAll.checked = false
        show()
        raise()
        requestActivate()
    }

    DragHandler {
        parent: control.contentItem
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        grabPermissions: PointerHandler.CanTakeOverFromItems
                         | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                         | PointerHandler.ApprovesTakeOverByAnything
        onActiveChanged: if (active) control.controller.startSystemMove()
    }

    // Dismissing the window is the same as Stop: the task must not stay
    // blocked on a question nobody can see anymore.
    onClosing: if (!_answered) answer(-1)
    onVisibilityChanged: function(visibility) {
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            showNormal()
    }
    function toggleMaximized() {}

    Shortcut {
        sequence: "Escape"
        onActivated: control.close()
    }

    ColumnLayout {
        id: layout
        // Not filled to the window: the window height follows this layout's implicit
        // height a polish later, and a filled layout would be spread over the old height.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: control.padding
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            FishUI.IconItem {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                Layout.alignment: Qt.AlignTop
                source: control.incoming && control.incoming.iconName ? control.incoming.iconName : "unknown"
            }

            Column {
                Layout.fillWidth: true
                Layout.rightMargin: 32
                spacing: 4

                FishUI.Label {
                    objectName: "conflictHeading"
                    width: control.textWidth
                    text: control.heading
                    textFormat: Text.PlainText
                    font: Qt.font({ family: FishUI.Theme.fontFamily, pixelSize: 13, weight: Font.DemiBold })
                    color: FishUI.Theme.textColor
                    wrapMode: Text.Wrap
                }
                FishUI.Label {
                    width: control.textWidth
                    text: control.question
                    textFormat: Text.PlainText
                    font: Qt.font({ family: FishUI.Theme.fontFamily, pixelSize: 12 })
                    color: FishUI.Theme.disabledTextColor
                    wrapMode: Text.Wrap
                }
            }
        }

        RowLayout {
            id: actions
            Layout.fillWidth: true
            spacing: 8

            FishUI.CheckBox {
                id: applyToAll
                objectName: "conflictApplyToAll"
                visible: control.sourceCount > 1
                focusPolicy: Qt.NoFocus
                text: qsTr("Apply to All")
                padding: 0
            }
            Item { Layout.fillWidth: true }

            FishUI.ConfirmDialogButton {
                objectName: "conflictStopButton"
                text: qsTr("Stop")
                onClicked: control.answer(-1)
            }
            FishUI.ConfirmDialogButton {
                objectName: "conflictSkipButton"
                visible: control.sourceCount > 1
                text: qsTr("Skip")
                onClicked: control.answer(1)
            }
            FishUI.ConfirmDialogButton {
                objectName: "conflictKeepBothButton"
                text: qsTr("Keep Both")
                onClicked: control.answer(2)
            }
            FishUI.ConfirmDialogButton {
                objectName: "conflictReplaceButton"
                text: qsTr("Replace")
                primary: true
                destructive: true
                onClicked: control.answer(3)
            }
        }
    }
}
