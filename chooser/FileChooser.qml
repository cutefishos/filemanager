import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FishUI 1.0 as FishUI
import Cutefish.FileManager 1.0
import "../qml/Controls" as FileManagerControls
import "../qml/FileList" as FileList
import "../qml" as FileManager

FishUI.Window {
    id: root
    objectName: "fileChooserWindow"
    width: 860
    height: 520
    minimumWidth: 860
    maximumWidth: 860
    minimumHeight: 520
    maximumHeight: 520
    visible: false
    title: chooser.title
    flags: Qt.Dialog | Qt.FramelessWindowHint
    minimizeButtonVisible: false
    closeButtonVisible: false
    // No title bar: a dialog this small cannot spare the vertical space, and
    // Escape or Cancel closes it. The path bar drags the window instead.
    header.height: 0
    contentTopMargin: 0
    // Opaque on purpose: the chooser does not use the blurred window background.
    background.opacity: 1
    background.color: FishUI.Theme.backgroundColor
    onClosing: chooser.cancel()

    readonly property bool canCreateFolder: chooser.saveMode || chooser.directoryMode

    // Shares column widths and sort order with the file manager's list view.
    FileManager.GlobalSettings {
        id: listSettings
    }

    Binding { target: chooser; property: "sortMode"; value: listSettings.sortMode }
    Binding { target: chooser; property: "sortDesc"; value: listSettings.sortDesc }

    // FishUI's title-bar double click otherwise calls this even on fixed-size windows.
    function toggleMaximized() {}

    Shortcut { sequence: "Escape"; onActivated: chooser.cancel() }
    Shortcut { sequence: "Alt+Up"; onActivated: chooser.up() }
    Shortcut { sequence: "Alt+Left"; onActivated: chooser.back() }
    Shortcut { sequence: "Alt+Right"; onActivated: chooser.forward() }
    Shortcut { sequence: "Ctrl+H"; onActivated: chooser.showHidden = !chooser.showHidden }
    Shortcut { sequence: "Ctrl+L"; onActivated: pathBar.openEditor() }
    Shortcut { sequence: "Ctrl+Shift+N"; enabled: root.canCreateFolder; onActivated: newFolderDialog.begin() }

    Connections {
        target: chooser
        function onLocationChanged() {
            files.currentIndex = -1
            pathBarModel.url = chooser.directory
            pathBar.closeEditor()
        }
        function onConfirmOverwrite(name) {
            overwriteLabel.text = qsTr("“%1” already exists. Replace it?").arg(name)
            overwriteDialog.open()
        }
    }

    // The editor keeps focus until something takes it away, so a click outside
    // has to close it explicitly; the press that does so is consumed.
    MouseArea {
        id: editorDismissArea
        anchors.fill: parent
        z: 100
        enabled: pathEditor.visible
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onPressed: function(mouse) {
            var inside = pathBar.mapFromItem(editorDismissArea, mouse.x, mouse.y)
            mouse.accepted = inside.x < 0 || inside.y < 0
                    || inside.x > pathBar.width || inside.y > pathBar.height
            if (mouse.accepted)
                pathBar.closeEditor()
        }
    }

    // Stands in for the missing title bar. It has no MouseArea, so presses fall
    // through to whatever is underneath and only a real drag moves the window.
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: FishUI.Units.largeSpacing * 2
        z: 50

        DragHandler {
            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            grabPermissions: PointerHandler.CanTakeOverFromItems
                             | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                             | PointerHandler.ApprovesTakeOverByAnything
            onActiveChanged: if (active && root.helper) root.helper.startSystemMove(root)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: sideBar
            Layout.preferredWidth: 180
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.backgroundColor, 1.35)
                                             : Qt.darker(FishUI.Theme.backgroundColor, 1.04)
            }

            PlacesModel {
                id: places
                onDeviceSetupDone: function(filePath) { chooser.navigate(filePath) }
                onDeviceOperationFailed: function(message) {
                    deviceError.text = message
                    deviceError.open()
                }
            }

            ListView {
                id: placesView
                anchors.fill: parent
                anchors.topMargin: FishUI.Units.largeSpacing
                anchors.bottomMargin: FishUI.Units.smallSpacing
                model: places
                clip: true
                spacing: 3
                boundsBehavior: Flickable.StopAtBounds
                interactive: false

                readonly property real sideInset: FishUI.Units.smallSpacing * 1.5
                readonly property real rowHeight: Math.max(34, FishUI.Units.fontMetrics.height + 12)

                FishUI.WheelHandler { target: placesView }
                ScrollBar.vertical: ScrollBar {}

                header: Label {
                    width: placesView.width
                    height: 32
                    leftPadding: placesView.sideInset + FishUI.Units.smallSpacing
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Favorites")
                    color: FishUI.Theme.disabledTextColor
                    font.pointSize: 9
                    font.bold: true
                }

                section.property: "category"
                section.delegate: Label {
                    width: ListView.view.width
                    height: !section || section === qsTr("Favorites") ? 0 : 36
                    text: section === qsTr("Favorites") ? "" : section
                    leftPadding: placesView.sideInset + FishUI.Units.smallSpacing
                    verticalAlignment: Text.AlignVCenter
                    color: FishUI.Theme.disabledTextColor
                    font.pointSize: 9
                    font.bold: true
                }

                delegate: Item {
                    id: place
                    required property int index
                    required property var model
                    // Remote places cannot be handed to the portal as local paths.
                    readonly property bool usable: model.url.toString().indexOf("file:") === 0 || model.setupNeeded
                    readonly property bool checked: chooser.directory === model.path

                    width: ListView.view.width
                    height: usable ? placesView.rowHeight : 0
                    visible: usable

                    MouseArea {
                        id: placePointer
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (place.model.setupNeeded)
                                places.requestSetup(place.index)
                            else
                                chooser.navigate(place.model.path)
                        }
                    }

                    Rectangle {
                        x: placesView.sideInset
                        width: place.width - placesView.sideInset * 2
                        height: parent.height
                        radius: FishUI.Theme.mediumRadius
                        color: placePointer.pressed ? Qt.rgba(FishUI.Theme.textColor.r,
                                                             FishUI.Theme.textColor.g,
                                                             FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1)
                             : placePointer.containsMouse && !place.checked ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                                      FishUI.Theme.textColor.g,
                                                                                      FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.1 : 0.05)
                             : place.checked ? Qt.rgba(FishUI.Theme.textColor.r,
                                                       FishUI.Theme.textColor.g,
                                                       FishUI.Theme.textColor.b, 0.05) : "transparent"
                        smooth: true
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: placesView.sideInset + FishUI.Units.smallSpacing
                        anchors.rightMargin: placesView.sideInset + FishUI.Units.smallSpacing
                        spacing: FishUI.Units.smallSpacing

                        FishUI.IconItem {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            themeSize: 20
                            source: place.model.iconName
                            color: place.checked ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                        }

                        Label {
                            Layout.fillWidth: true
                            text: place.model.name
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: place.checked ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                color: FishUI.Theme.secondBackgroundColor
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: FishUI.Units.largeSpacing
                    spacing: FishUI.Units.smallSpacing

                    FileManagerControls.IconButton {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        backgroundColor: FishUI.Theme.alternateBackgroundColor
                        hoveredColor: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.alternateBackgroundColor, 1.3)
                                                            : Qt.darker(FishUI.Theme.alternateBackgroundColor, 1.08)
                        pressedColor: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.alternateBackgroundColor, 1.15)
                                                            : Qt.darker(FishUI.Theme.alternateBackgroundColor, 1.16)
                        source: "qrc:/images/" + (FishUI.Theme.darkMode ? "dark/" : "light/") + "go-previous.svg"
                        enabled: chooser.canGoBack
                        Accessible.name: qsTr("Back")
                        onClicked: chooser.back()
                    }

                    FileManagerControls.IconButton {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        backgroundColor: FishUI.Theme.alternateBackgroundColor
                        hoveredColor: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.alternateBackgroundColor, 1.3)
                                                            : Qt.darker(FishUI.Theme.alternateBackgroundColor, 1.08)
                        pressedColor: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.alternateBackgroundColor, 1.15)
                                                            : Qt.darker(FishUI.Theme.alternateBackgroundColor, 1.16)
                        source: "qrc:/images/" + (FishUI.Theme.darkMode ? "dark/" : "light/") + "go-next.svg"
                        enabled: chooser.canGoForward
                        Accessible.name: qsTr("Forward")
                        onClicked: chooser.forward()
                    }

                    // Breadcrumbs with the file manager's path bar look; clicking an
                    // empty spot or Ctrl+L swaps in the editable path.
                    Item {
                        id: pathBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30

                        function openEditor() {
                            pathEditor.text = pathBarModel.url
                            pathEditor.visible = true
                            crumbs.visible = false
                            pathEditor.forceActiveFocus()
                            pathEditor.selectAll()
                        }

                        function closeEditor() {
                            if (!pathEditor.visible)
                                return
                            pathEditor.visible = false
                            crumbs.visible = true
                            files.forceActiveFocus()
                        }

                        PathBarModel {
                            id: pathBarModel
                            url: chooser.directory
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: FishUI.Theme.smallRadius
                            color: FishUI.Theme.alternateBackgroundColor
                        }

                        // Dragging the path bar moves the window, which has no title
                        // bar; the click that opens the editor must still get through.
                        property bool dragging: false

                        MouseArea {
                            anchors.fill: parent
                            onPressed: pathBar.dragging = false
                            onClicked: if (!pathBar.dragging) pathBar.openEditor()
                        }

                        DragHandler {
                            target: null
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                            grabPermissions: PointerHandler.CanTakeOverFromItems
                                             | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                                             | PointerHandler.ApprovesTakeOverByAnything
                            onActiveChanged: {
                                if (!active)
                                    return
                                pathBar.dragging = true
                                if (root.helper)
                                    root.helper.startSystemMove(root)
                            }
                        }

                        ListView {
                            id: crumbs
                            anchors.fill: parent
                            anchors.topMargin: 2
                            anchors.bottomMargin: 2
                            orientation: Qt.Horizontal
                            interactive: false
                            clip: true
                            leftMargin: 3
                            rightMargin: 3
                            spacing: FishUI.Units.smallSpacing
                            model: pathBarModel
                            currentIndex: count - 1

                            onCountChanged: positionViewAtEnd()

                            delegate: MouseArea {
                                id: crumb
                                required property int index
                                required property var model
                                readonly property bool current: index === crumbs.count - 1

                                height: crumbs.height - crumbs.topMargin - crumbs.bottomMargin
                                width: crumbLabel.width + FishUI.Units.largeSpacing
                                hoverEnabled: true
                                onPressed: pathBar.dragging = false
                                onClicked: if (!pathBar.dragging) chooser.navigate(model.path)

                                Rectangle {
                                    anchors.fill: parent
                                    radius: FishUI.Theme.smallRadius
                                    color: crumb.pressed ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                   FishUI.Theme.textColor.g,
                                                                   FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1)
                                         : crumb.containsMouse ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                         FishUI.Theme.textColor.g,
                                                                         FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.1 : 0.05)
                                         : "transparent"
                                    smooth: true
                                }

                                Label {
                                    id: crumbLabel
                                    anchors.centerIn: parent
                                    text: crumb.model.name
                                    textFormat: Text.PlainText
                                    color: crumb.current ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                                }
                            }
                        }

                        TextField {
                            id: pathEditor
                            objectName: "locationField"
                            anchors.fill: parent
                            visible: false
                            selectByMouse: true
                            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                            Accessible.name: qsTr("Location")
                            onAccepted: chooser.navigate(text)
                            onActiveFocusChanged: if (!activeFocus) pathBar.closeEditor()
                            Keys.onEscapePressed: pathBar.closeEditor()
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: files
                        objectName: "fileList"
                        anchors.fill: parent
                        model: chooser
                        clip: true
                        focus: true
                        currentIndex: -1
                        keyNavigationEnabled: false
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: false
                        topMargin: columns.headerHeight + 2
                        leftMargin: FishUI.Units.smallSpacing
                        rightMargin: FishUI.Units.largeSpacing
                        bottomMargin: FishUI.Units.smallSpacing

                        readonly property FileList.Columns columns: FileList.Columns {
                            settings: listSettings
                            availableWidth: Math.max(0, files.width - files.leftMargin - files.rightMargin)
                        }

                        FishUI.WheelHandler { target: files }
                        ScrollBar.vertical: ScrollBar {
                            topPadding: files.columns.headerHeight
                        }

                        function moveCurrentIndex(newIndex, modifiers) {
                            if (count === 0)
                                return
                            currentIndex = Math.max(0, Math.min(count - 1, newIndex))
                            chooser.select(currentIndex, modifiers)

                            var top = currentIndex * columns.rowHeight
                            if (top < contentY + columns.headerHeight)
                                contentY = top - columns.headerHeight
                            else if (top + columns.rowHeight > contentY + height - bottomMargin)
                                contentY = top + columns.rowHeight - height + bottomMargin
                        }

                        Keys.onPressed: function(event) {
                            var page = Math.max(1, Math.floor((height - columns.headerHeight) / columns.rowHeight) - 1)

                            if (event.key === Qt.Key_Down) {
                                moveCurrentIndex(currentIndex + 1, event.modifiers)
                            } else if (event.key === Qt.Key_Up) {
                                moveCurrentIndex(currentIndex === -1 ? 0 : currentIndex - 1, event.modifiers)
                            } else if (event.key === Qt.Key_Home) {
                                moveCurrentIndex(0, event.modifiers)
                            } else if (event.key === Qt.Key_End) {
                                moveCurrentIndex(count - 1, event.modifiers)
                            } else if (event.key === Qt.Key_PageUp) {
                                moveCurrentIndex(Math.max(0, currentIndex) - page, event.modifiers)
                            } else if (event.key === Qt.Key_PageDown) {
                                moveCurrentIndex(currentIndex + page, event.modifiers)
                            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                if (chooser.multiple)
                                    chooser.accept()
                                else
                                    chooser.activate(currentIndex)
                            } else if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) {
                                chooser.selectAll()
                            } else {
                                return
                            }
                            event.accepted = true
                        }

                        // Rows scroll beneath the header, so it blocks input over itself.
                        FileList.Header {
                            z: 2
                            width: files.width
                            columns: files.columns
                            leftInset: files.leftMargin
                            rightInset: files.rightMargin
                            backgroundColor: root.background.color
                        }

                        delegate: MouseArea {
                            id: entry
                            required property int index
                            required property var model

                            readonly property bool selected: model.selected
                            readonly property bool previousSelected: {
                                chooser.selectionSummary
                                return selected && chooser.isSelected(index - 1)
                            }
                            readonly property bool nextSelected: {
                                chooser.selectionSummary
                                return selected && chooser.isSelected(index + 1)
                            }

                            width: files.columns.availableWidth
                            height: files.columns.rowHeight
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton

                            Accessible.name: model.fileName
                            Accessible.role: Accessible.Row

                            onClicked: function(mouse) {
                                files.currentIndex = entry.index
                                files.forceActiveFocus()
                                chooser.select(entry.index, mouse.modifiers)
                            }
                            onDoubleClicked: chooser.activate(entry.index)

                            FileList.Row {
                                anchors.fill: parent
                                columns: files.columns
                                itemModel: entry.model
                                rowIndex: entry.index
                                selected: entry.selected
                                hovered: entry.containsMouse
                                previousSelected: entry.previousSelected
                                nextSelected: entry.nextSelected
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !chooser.busy && files.count === 0
                        text: qsTr("Empty folder")
                        font.pointSize: 15
                        color: FishUI.Theme.disabledTextColor
                    }

                    BusyIndicator { anchors.centerIn: parent; running: chooser.busy; visible: running }
                }

                ColumnLayout {
                    id: fieldsBand
                    Layout.fillWidth: true
                    Layout.topMargin: FishUI.Units.largeSpacing
                    Layout.leftMargin: FishUI.Units.largeSpacing
                    Layout.rightMargin: FishUI.Units.largeSpacing
                    visible: chooser.saveMode || chooser.filters.length > 0 || chooser.choices.length > 0
                    spacing: FishUI.Units.smallSpacing

                    // Keeps the fields of both rows on one left edge.
                    readonly property real labelWidth: Math.max(saveAsLabel.implicitWidth, filterLabel.implicitWidth)

                    RowLayout {
                        Layout.fillWidth: true
                        visible: chooser.saveMode
                        spacing: FishUI.Units.smallSpacing

                        Label {
                            id: saveAsLabel
                            Layout.preferredWidth: fieldsBand.labelWidth
                            text: qsTr("Save As:")
                            color: FishUI.Theme.textColor
                        }

                        TextField {
                            id: nameField
                            objectName: "fileNameField"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            text: chooser.fileName
                            selectByMouse: true
                            onTextEdited: chooser.fileName = text
                            onAccepted: chooser.accept()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: chooser.filters.length > 0
                        spacing: FishUI.Units.smallSpacing

                        Label {
                            id: filterLabel
                            Layout.preferredWidth: fieldsBand.labelWidth
                            text: qsTr("File Type:")
                            color: FishUI.Theme.textColor
                        }

                        FishUI.ComboBox {
                            objectName: "filterCombo"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            model: chooser.filters
                            textRole: "name"
                            currentIndex: chooser.filterIndex
                            onActivated: chooser.filterIndex = currentIndex
                            Accessible.name: qsTr("File type")
                        }
                    }

                    Repeater {
                        model: chooser.choices

                        delegate: RowLayout {
                            required property var modelData
                            readonly property bool isToggle: modelData.options.length === 0

                            Layout.fillWidth: true
                            spacing: FishUI.Units.smallSpacing

                            Label {
                                visible: !parent.isToggle
                                text: parent.modelData.label
                                textFormat: Text.PlainText
                                color: FishUI.Theme.textColor
                            }

                            FishUI.ComboBox {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                visible: !parent.isToggle
                                model: parent.modelData.options
                                textRole: "label"
                                valueRole: "id"
                                Component.onCompleted: currentIndex = Math.max(0, indexOfValue(parent.modelData.selected))
                                onActivated: chooser.setChoice(parent.modelData.id, currentValue)
                            }

                            CheckBox {
                                visible: parent.isToggle
                                text: parent.modelData.label
                                checked: parent.modelData.selected === "true"
                                onToggled: chooser.setChoice(parent.modelData.id, checked ? "true" : "false")
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: FishUI.Units.largeSpacing
                    Layout.leftMargin: FishUI.Units.largeSpacing
                    Layout.rightMargin: FishUI.Units.largeSpacing
                    visible: chooser.error.length > 0
                    text: chooser.error
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    color: FishUI.Theme.darkMode ? "#ff453a" : "#d92d20"
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: FishUI.Units.largeSpacing * 2
                    Layout.leftMargin: FishUI.Units.largeSpacing
                    Layout.rightMargin: FishUI.Units.largeSpacing
                    Layout.bottomMargin: FishUI.Units.largeSpacing
                    spacing: FishUI.Units.smallSpacing

                    FishUI.ConfirmDialogButton {
                        text: qsTr("New Folder")
                        visible: root.canCreateFolder
                        onClicked: newFolderDialog.begin()
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: FishUI.Units.smallSpacing
                        visible: chooser.multiple
                        text: chooser.selectionSummary
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        color: FishUI.Theme.disabledTextColor
                    }

                    Item { Layout.fillWidth: true }

                    FishUI.ConfirmDialogButton {
                        objectName: "cancelButton"
                        text: qsTr("Cancel")
                        onClicked: chooser.cancel()
                    }

                    FishUI.ConfirmDialogButton {
                        objectName: "acceptButton"
                        text: chooser.acceptLabel
                        primary: true
                        onClicked: chooser.accept()
                    }
                }
            }
        }
    }

    Dialog {
        id: overwriteDialog
        anchors.centerIn: parent
        width: Math.min(420, root.width - 40)
        modal: true
        title: qsTr("Replace File?")
        contentItem: Label { id: overwriteLabel; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        footer: RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            FishUI.ConfirmDialogButton {
                text: qsTr("Cancel")
                Layout.bottomMargin: 16
                onClicked: overwriteDialog.reject()
            }
            FishUI.ConfirmDialogButton {
                text: qsTr("Replace")
                primary: true
                destructive: true
                Layout.rightMargin: 16
                Layout.bottomMargin: 16
                onClicked: overwriteDialog.accept()
            }
        }
        onAccepted: chooser.accept(true)
    }
    Dialog {
        id: newFolderDialog
        anchors.centerIn: parent
        width: 340
        modal: true
        title: qsTr("New Folder")

        function begin() {
            folderName.text = ""
            open()
            folderName.forceActiveFocus()
        }

        contentItem: TextField {
            id: folderName
            placeholderText: qsTr("Folder name")
            selectByMouse: true
            onAccepted: if (text.length > 0) newFolderDialog.accept()
        }
        footer: RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            FishUI.ConfirmDialogButton {
                text: qsTr("Cancel")
                Layout.bottomMargin: 16
                onClicked: newFolderDialog.reject()
            }
            FishUI.ConfirmDialogButton {
                text: qsTr("Create")
                primary: true
                enabled: folderName.text.length > 0
                Layout.rightMargin: 16
                Layout.bottomMargin: 16
                onClicked: newFolderDialog.accept()
            }
        }
        onAccepted: chooser.newFolder(folderName.text)
    }
    Dialog {
        id: deviceError
        property alias text: deviceErrorLabel.text
        anchors.centerIn: parent
        width: 360
        modal: true
        title: qsTr("Device unavailable")
        contentItem: Label { id: deviceErrorLabel; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            FishUI.ConfirmDialogButton {
                text: qsTr("Close")
                primary: true
                Layout.rightMargin: 16
                Layout.bottomMargin: 16
                onClicked: deviceError.accept()
            }
        }
    }
}
