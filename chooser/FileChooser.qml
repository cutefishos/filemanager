import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FishUI 1.0 as FishUI
import Cutefish.Application
import Cutefish.FileManager 1.0
import "../qml/Controls" as FileManagerControls
import "../qml/FileList" as FileList
import "../qml" as FileManager

FishUI.Window {
    id: root
    WindowHints.skipTaskbar: true
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
            overwriteDialog.text = qsTr("“%1” already exists. Replace it?").arg(name)
            overwriteDialog.open()
        }
    }

    // The editor keeps focus until something takes it away, so a press outside
    // has to close it explicitly. A PointHandler only watches: the press still
    // reaches its target, and without a cursorShape it leaves the cursor alone.
    Item {
        anchors.fill: parent
        z: 100

        PointHandler {
            enabled: pathEditor.visible
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            onActiveChanged: {
                if (active && !pathBar.contains(pathBar.mapFromItem(null, point.scenePressPosition)))
                    pathBar.closeEditor()
            }
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
            onActiveChanged: if (active && root.controller) root.controller.startSystemMove()
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
                FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

                header: FishUI.Label {
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
                section.delegate: FishUI.Label {
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

                        FishUI.Label {
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
                    spacing: FishUI.Units.smallSpacing / 2

                    FileManagerControls.IconButton {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        iconName: "chevron-left"
                        iconSize: 16
                        enabled: chooser.canGoBack
                        Accessible.name: qsTr("Back")
                        onClicked: chooser.back()
                    }

                    FileManagerControls.IconButton {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        iconName: "chevron-right"
                        iconSize: 16
                        enabled: chooser.canGoForward
                        Accessible.name: qsTr("Forward")
                        onClicked: chooser.forward()
                    }

                    // The file manager's path bar; clicking an empty spot or
                    // Ctrl+L swaps in the editable path.
                    Item {
                        id: pathBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        Layout.leftMargin: FishUI.Units.smallSpacing / 2

                        readonly property color ancestorColor: Qt.rgba(FishUI.Theme.textColor.r,
                                                                      FishUI.Theme.textColor.g,
                                                                      FishUI.Theme.textColor.b, 0.6)

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
                                if (root.controller)
                                    root.controller.startSystemMove()
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
                            // With the crumb's own half-largeSpacing, text starts where the editor's does.
                            leftMargin: FishUI.Units.smallSpacing
                            rightMargin: FishUI.Units.smallSpacing
                            spacing: 10 + FishUI.Units.smallSpacing
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
                                onPressed: pathBar.dragging = false
                                onClicked: if (!pathBar.dragging) chooser.navigate(model.path)

                                Rectangle {
                                    anchors.fill: parent
                                    radius: FishUI.Theme.smallRadius
                                    color: crumb.pressed ? Qt.rgba(FishUI.Theme.textColor.r,
                                                                   FishUI.Theme.textColor.g,
                                                                   FishUI.Theme.textColor.b, FishUI.Theme.darkMode ? 0.05 : 0.1)
                                         : "transparent"
                                    smooth: true
                                }

                                FishUI.Label {
                                    id: crumbLabel
                                    anchors.centerIn: parent
                                    text: crumb.model.name
                                    textFormat: Text.PlainText
                                    color: crumb.current ? FishUI.Theme.textColor : pathBar.ancestorColor
                                }

                                FishUI.Icon {
                                    anchors.left: parent.right
                                    anchors.leftMargin: FishUI.Units.smallSpacing / 2
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 10
                                    height: 10
                                    visible: !crumb.current
                                    name: "chevron-right"
                                    color: pathBar.ancestorColor
                                }
                            }
                        }

                        FishUI.TextField {
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
                        FishUI.ScrollBar.vertical: FishUI.ScrollBar {
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
                                if (chooser.multiple || currentIndex < 0)
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
                                previousSelected: entry.previousSelected
                                nextSelected: entry.nextSelected
                            }
                        }
                    }

                    FishUI.Label {
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

                        FishUI.Label {
                            id: saveAsLabel
                            Layout.preferredWidth: fieldsBand.labelWidth
                            text: qsTr("Save As:")
                            color: FishUI.Theme.textColor
                        }

                        FishUI.TextField {
                            id: nameField
                            objectName: "fileNameField"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            text: chooser.fileName
                            selectByMouse: true
                            onTextEdited: chooser.fileName = text
                            onAccepted: chooser.accept()

                            // Saving starts here with the stem selected, so typing renames and Enter saves.
                            Component.onCompleted: {
                                if (!chooser.saveMode)
                                    return
                                forceActiveFocus()
                                const dot = text.lastIndexOf(".")
                                select(0, dot > 0 ? dot : text.length)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: chooser.filters.length > 0
                        spacing: FishUI.Units.smallSpacing

                        FishUI.Label {
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

                            FishUI.Label {
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

                            FishUI.CheckBox {
                                visible: parent.isToggle
                                text: parent.modelData.label
                                checked: parent.modelData.selected === "true"
                                onToggled: chooser.setChoice(parent.modelData.id, checked ? "true" : "false")
                            }
                        }
                    }
                }

                FishUI.Label {
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

                    FishUI.Label {
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

    FishUI.ConfirmDialog {
        id: overwriteDialog
        parentWindow: root
        x: root.x + Math.round((root.width - width) / 2)
        y: root.y + Math.round((root.height - height) / 2)
        showOnCompleted: false
        title: qsTr("Replace File?")
        confirmText: qsTr("Replace")
        destructive: true
        onAccepted: chooser.accept(true)
    }
    FishUI.Window {
        id: newFolderDialog
        parentWindow: root
        x: root.x + Math.round((root.width - width) / 2)
        y: root.y + Math.round((root.height - height) / 2)
        width: 340
        property int contentHeight: layout.implicitHeight + 32
        height: contentHeight
        minimumWidth: width
        maximumWidth: width
        minimumHeight: contentHeight
        maximumHeight: contentHeight
        flags: Qt.Dialog | Qt.FramelessWindowHint
        modality: Qt.WindowModal
        visible: false
        minimizeButtonVisible: false
        header.height: 44
        contentTopMargin: 0
        background.color: FishUI.Theme.secondBackgroundColor
        title: qsTr("New Folder")

        function begin() {
            folderName.clear()
            show()
            raise()
            requestActivate()
            folderName.forceActiveFocus()
        }

        function accept() {
            if (folderName.text.length === 0)
                return
            const name = folderName.text
            close()
            chooser.newFolder(name)
        }

        Shortcut {
            sequence: "Escape"
            onActivated: newFolderDialog.close()
        }

        ColumnLayout {
            id: layout
            anchors.fill: parent
            anchors.margins: 16
            spacing: 16

            FishUI.Label {
                Layout.fillWidth: true
                Layout.rightMargin: 32
                text: newFolderDialog.title
                textFormat: Text.PlainText
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: FishUI.Theme.textColor
            }

            FishUI.TextField {
                id: folderName
                Layout.fillWidth: true
                placeholderText: qsTr("Folder name")
                selectByMouse: true
                onAccepted: newFolderDialog.accept()
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                FishUI.ConfirmDialogButton {
                    text: qsTr("Cancel")
                    onClicked: newFolderDialog.close()
                }

                FishUI.ConfirmDialogButton {
                    text: qsTr("Create")
                    primary: true
                    enabled: folderName.text.length > 0
                    onClicked: newFolderDialog.accept()
                }
            }
        }
    }
    FishUI.ConfirmDialog {
        id: deviceError
        parentWindow: root
        x: root.x + Math.round((root.width - width) / 2)
        y: root.y + Math.round((root.height - height) / 2)
        showOnCompleted: false
        title: qsTr("Device unavailable")
        confirmText: qsTr("Close")
        cancelButtonVisible: false
    }
}
