import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FishUI 1.0 as FishUI
import Cutefish.Application

FishUI.Window {
    id: preview
    WindowHints.skipTaskbar: true
    WindowHints.alwaysOnTop: true
    // Fitting the window to each image keeps it where it is on screen.
    WindowHints.resizeAroundCenter: true
    objectName: "quickLookWindow"

    required property var controller
    // What is on screen. It trails controller.file while the next image
    // decodes, so the size, title and picture change in one frame.
    property var shown: ({})
    property var pending: null
    // Set from Space until the first item is on screen.
    property bool opening: false
    readonly property bool imageMode: shown.kind === "image"
    readonly property bool multiple: controller.items.length > 1
    readonly property var application: shown.application || null
    readonly property int sideMargin: 16
    readonly property int headerHeight: 40
    readonly property int minimumImageWidth: 400
    readonly property int minimumImageHeight: 200
    // The requesting window's: it opens on the same output, and until this
    // window has been on screen its own ratio is the rounded-up integer scale.
    readonly property real displayRatio: controller.sourceRatio > 0 ? controller.sourceRatio
                                         : FishUI.Dpi.ratio > 0 ? FishUI.Dpi.ratio : 1

    title: shown.name || qsTr("Quick Look")
    flags: Qt.Tool | Qt.FramelessWindowHint
    visible: false
    minimizeButtonVisible: false
    header.height: headerHeight
    windowButtonsAlignment: Qt.AlignVCenter
    windowButtonsTopMargin: 0
    windowButtonsRightMargin: 4
    contentTopMargin: 0
    modality: Qt.NonModal
    restorationBehavior: FishUI.Window.Disabled
    width: 480
    height: 144
    minimumWidth: 480
    maximumWidth: 480
    minimumHeight: 144
    maximumHeight: 144
    blurEnabled: false
    background.visible: !glassEffect.available
    background.color: FishUI.Theme.backgroundColor

    FishUI.GlassEffect {
        id: glassEffect
        view: preview
        windowRadius: FishUI.Theme.windowRadius
        blurSigma: 4.4
        displacement: 21.6
        chroma: 0.03
        tint: FishUI.Theme.darkMode ? Qt.rgba(0.18, 0.18, 0.20, 0.7)
                                    : Qt.rgba(1.0, 1.0, 1.0, 0.7)
    }

    function isImage(file) {
        return file.kind === "image" && file.width > 0 && file.height > 0
    }

    // An image at its own pixels, shrunk to fit a large share of the screen.
    function sizeFor(file) {
        if (file.kind !== "image")
            return Qt.size(480, multiple ? 168 : 144)
        if (!isImage(file))
            return Qt.size(720, 540)
        const ratio = preview.displayRatio
        const w = file.width / ratio
        const h = file.height / ratio
        const scale = Math.min(1, Math.floor(Screen.width * 0.75) / w,
                               (Math.floor(Screen.height * 0.8) - headerHeight) / h)
        return Qt.size(Math.max(minimumImageWidth, Math.round(w * scale)),
                       Math.max(minimumImageHeight, Math.round(h * scale)) + headerHeight)
    }

    // The decode PreviewImageView asks for when fitted into a window of `size`.
    function fittedPixels(file, size) {
        const ratio = preview.displayRatio
        const zoom = Math.min(1, size.width * ratio / file.width,
                              (size.height - headerHeight) * ratio / file.height)
        return Qt.size(Math.ceil(file.width * zoom), Math.ceil(file.height * zoom))
    }

    // Decodes an image into the pixmap cache before it is shown.
    function stage(file) {
        if (!isImage(file) || file.animated) {
            present(file)
            return
        }
        // Configured before `pending` is set: a new sourceSize alone reports a status.
        preloader.sourceSize = fittedPixels(file, sizeFor(file))
        preloader.source = file.url
        pending = file
        if (preloader.status !== Image.Loading)
            present(file)
        else if (!stall.running)
            stall.start()
    }

    function present(file) {
        pending = null
        stall.stop()
        const size = sizeFor(file)
        const image = Object.assign({}, file)
        if (isImage(file) && !file.animated)
            image.prepared = fittedPixels(file, size)
        // The contents first: a resize can render a frame before returning.
        shown = image
        // Opened up first: Qt bounds a resize by the old limits.
        minimumWidth = 0
        minimumHeight = 0
        maximumWidth = 16777215
        maximumHeight = 16777215
        // One geometry change: separate width and height changes each render a frame.
        setGeometry(x, y, size.width, size.height)
        minimumWidth = size.width
        maximumWidth = size.width
        minimumHeight = size.height
        maximumHeight = size.height
        preloader.source = ""
        if (opening) {
            opening = false
            showNormal()
            content.forceActiveFocus()
            requestActivate()
        }
    }

    // Its source, size, orientation and fill mode match PreviewImageView's
    // fitted layer: together they are the pixmap cache key.
    Image {
        id: preloader
        visible: false
        asynchronous: true
        autoTransform: true
        fillMode: Image.PreserveAspectFit
        onStatusChanged: {
            if (preview.pending && status !== Image.Loading)
                preview.present(preview.pending)
        }
    }

    // A slow decode is shown anyway, with the busy indicator.
    Timer {
        id: stall
        interval: 300
        onTriggered: if (preview.pending) preview.present(preview.pending)
    }

    onClosing: function(close) {
        close.accepted = false
        controller.close()
    }

    Connections {
        target: controller
        function onShowRequested() {
            preview.opening = true
            preview.stage(controller.file)
        }
        function onChanged() {
            if (!controller.visible) {
                preview.pending = null
                preview.opening = false
                stall.stop()
                preview.hide()
            }
        }
        function onFileChanged() {
            if (preview.visible && !controller.loading)
                preview.stage(controller.file)
        }
    }

    headerItem: Item {
        RowLayout {
            id: navigation
            visible: preview.multiple
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: preview.windowButtonsRightMargin
            spacing: 0

            FishUI.HeaderButton {
                size: 32
                iconSize: 16
                Layout.preferredWidth: size
                Layout.preferredHeight: size
                Layout.alignment: Qt.AlignVCenter
                iconName: "chevron-left"
                Accessible.name: qsTr("Previous item")
                enabled: controller.currentIndex > 0
                onClicked: controller.step(-1)
            }
            FishUI.HeaderButton {
                size: 32
                iconSize: 16
                Layout.preferredWidth: size
                Layout.preferredHeight: size
                Layout.alignment: Qt.AlignVCenter
                iconName: "chevron-right"
                Accessible.name: qsTr("Next item")
                enabled: controller.currentIndex + 1 < controller.items.length
                onClicked: controller.step(1)
            }
            FishUI.Label {
                Layout.leftMargin: 4
                Layout.alignment: Qt.AlignVCenter
                verticalAlignment: Text.AlignVCenter
                text: qsTr("%1 of %2").arg(controller.currentIndex + 1).arg(controller.items.length)
                color: FishUI.Theme.textColor
            }
        }

        FishUI.Label {
            id: titleLabel
            objectName: "previewTitle"
            visible: preview.imageMode
            anchors.left: navigation.visible ? navigation.right : parent.left
            anchors.leftMargin: navigation.visible ? 8 : 12
            anchors.right: actions.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: preview.shown.name || ""
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            font.bold: true
            color: FishUI.Theme.textColor
        }

        RowLayout {
            id: actions
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 4
            spacing: 4

            Item {
                id: openButton
                // A single compact preview has no header row to hold it.
                visible: !!preview.application && (preview.imageMode || preview.multiple)
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: Math.min(220, openLabel.implicitWidth + 24)
                implicitHeight: 26
                Accessible.role: Accessible.Button
                Accessible.name: openLabel.text

                readonly property color tint: FishUI.Theme.darkMode ? "#FFFFFF" : "#323238"

                FishUI.RoundedRectangle {
                    anchors.fill: parent
                    continuousCorners: false
                    radius: height / 2
                    color: Qt.rgba(openButton.tint.r, openButton.tint.g, openButton.tint.b,
                                   openArea.pressed ? 0.21 : openArea.containsMouse ? 0.14 : 0.07)
                }
                FishUI.Label {
                    id: openLabel
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: preview.application ? qsTr("Open with %1").arg(preview.application.name) : ""
                    color: FishUI.Theme.textColor
                    opacity: preview.active ? 1.0 : 0.6
                }
                MouseArea {
                    id: openArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: controller.openCurrent()
                }
            }
        }
    }

    // Anchors rather than a layout: a layout settles in a later polish pass,
    // so the first frame at a new window size would show the old geometry.
    Item {
        id: content
        anchors.fill: parent
        focus: true

        Loader {
            id: renderer
            anchors.fill: parent
            anchors.topMargin: (preview.multiple || preview.imageMode ? preview.headerHeight : 0)
                               + (preview.imageMode ? 0 : preview.multiple ? 8 : 16)
            anchors.leftMargin: preview.imageMode ? 0 : preview.sideMargin
            anchors.rightMargin: preview.imageMode ? 0 : preview.multiple ? preview.sideMargin : 40
            anchors.bottomMargin: (preview.imageMode ? 0 : 16)
                                  + (message.visible ? message.height + 20 : 0)
            sourceComponent: preview.imageMode ? imagePreview : fileInfo
        }

        FishUI.Label {
            id: message
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            visible: !!preview.shown.message
            text: preview.shown.message || ""
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: FishUI.Theme.textColor
        }
    }

    Component {
        id: imagePreview

        Item {
            PreviewImageView {
                id: imageView
                anchors.fill: parent
                image: preview.shown
                ratio: preview.displayRatio
            }

            // Shown only for a decode slow enough to notice.
            FishUI.BusyIndicator {
                anchors.centerIn: parent
                visible: imageView.loading && busyDelay.elapsed

                Timer {
                    id: busyDelay
                    property bool elapsed: false
                    interval: 300
                    running: imageView.loading
                    onRunningChanged: if (running) elapsed = false
                    onTriggered: elapsed = true
                }
            }

            FishUI.Label {
                anchors.centerIn: parent
                visible: imageView.failed
                text: qsTr("This image could not be displayed.")
                color: FishUI.Theme.textColor
            }
        }
    }

    Component {
        id: fileInfo
        RowLayout {
            spacing: 20

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 96
                Layout.preferredHeight: 96
                objectName: "previewFileIcon"
                sourceSize: Qt.size(128, 128)
                source: "image://fileicon/" + (preview.shown.icon || "application-octet-stream")
                        + FishUI.Theme.iconThemeQuery
                fillMode: Image.PreserveAspectFit
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 6

                FishUI.Label {
                    objectName: "previewFileName"
                    Layout.fillWidth: true
                    text: preview.shown.name || ""
                    textFormat: Text.PlainText
                    wrapMode: Text.NoWrap
                    maximumLineCount: 1
                    elide: Text.ElideMiddle
                    font.bold: true
                    font.pixelSize: 16
                }
                FishUI.Label {
                    Layout.fillWidth: true
                    text: preview.shown.folder
                          ? qsTr("%1, %2").arg(preview.shown.size || "").arg(preview.shown.itemCount === 1
                              ? qsTr("1 item") : qsTr("%1 items").arg(preview.shown.itemCount || 0))
                          : preview.shown.size || ""
                }
            }
        }
    }
}
