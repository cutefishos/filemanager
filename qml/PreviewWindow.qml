import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import FishUI 1.0 as FishUI
import Cutefish.Application

FishUI.Window {
    id: preview
    WindowHints.skipTaskbar: true
    objectName: "quickLookWindow"

    required property var controller
    readonly property var file: controller.file
    readonly property bool compact: file.kind !== "image"
    readonly property bool multiple: controller.items.length > 1
    readonly property int sideMargin: 16
    readonly property int previewWidth: compact ? 480 : 720
    readonly property int previewHeight: compact ? (multiple ? 168 : 144) : 540

    title: qsTr("Quick Look")
    flags: Qt.Tool | Qt.FramelessWindowHint
    visible: false
    minimizeButtonVisible: false
    header.height: 40
    windowButtonsAlignment: Qt.AlignVCenter
    windowButtonsTopMargin: 0
    windowButtonsRightMargin: 4
    contentTopMargin: 0
    modality: Qt.NonModal
    width: previewWidth
    height: previewHeight
    minimumWidth: previewWidth
    maximumWidth: previewWidth
    minimumHeight: previewHeight
    maximumHeight: previewHeight
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

    onClosing: function(close) {
        close.accepted = false
        controller.close()
    }

    Connections {
        target: controller
        function onShowRequested() {
            if (preview.transientParent) {
                preview.x = preview.transientParent.x + (preview.transientParent.width - preview.width) / 2
                preview.y = preview.transientParent.y + (preview.transientParent.height - preview.height) / 2
            }
            preview.showNormal()
            content.forceActiveFocus()
            preview.requestActivate()
        }
        function onChanged() {
            if (!controller.visible)
                preview.hide()
        }
    }

    headerItem: Item {
        RowLayout {
            visible: preview.multiple
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: preview.windowButtonsRightMargin
            anchors.rightMargin: 8
            spacing: 0

            FishUI.RoundImageButton {
                size: 32
                iconSize: 16
                Layout.preferredWidth: size
                Layout.preferredHeight: size
                Layout.alignment: Qt.AlignVCenter
                opacity: enabled ? 1 : 0.35
                iconName: "chevron-left"
                Accessible.name: qsTr("Previous item")
                enabled: controller.currentIndex > 0
                onClicked: controller.step(-1)
            }
            FishUI.RoundImageButton {
                size: 32
                iconSize: 16
                Layout.preferredWidth: size
                Layout.preferredHeight: size
                Layout.alignment: Qt.AlignVCenter
                opacity: enabled ? 1 : 0.35
                iconName: "chevron-right"
                Accessible.name: qsTr("Next item")
                enabled: controller.currentIndex + 1 < controller.items.length
                onClicked: controller.step(1)
            }
            FishUI.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 4
                Layout.alignment: Qt.AlignVCenter
                verticalAlignment: Text.AlignVCenter
                text: controller.items.length > 1
                      ? qsTr("%1 of %2").arg(controller.currentIndex + 1).arg(controller.items.length) : ""
                color: FishUI.Theme.textColor
            }
        }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 0
        focus: true

        Item {
            visible: preview.multiple
            Layout.fillWidth: true
            Layout.preferredHeight: preview.header.height
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: preview.sideMargin
            Layout.rightMargin: preview.multiple ? preview.sideMargin : 40
            Layout.topMargin: preview.multiple ? 8 : 16
            Layout.bottomMargin: 16

            Loader {
                id: renderer
                anchors.fill: parent
                sourceComponent: {
                    if (file.kind === "image")
                        return imagePreview
                    return fileInfo
                }
            }
        }

        FishUI.Label {
            Layout.fillWidth: true
            Layout.margins: 10
            visible: !!file.message
            text: file.message || ""
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: FishUI.Theme.textColor
        }
    }

    Component {
        id: imagePreview
        Flickable {
            id: imageView
            objectName: "previewImageView"
            property real zoom: 1
            clip: true
            contentWidth: width * zoom
            contentHeight: height * zoom
            boundsBehavior: Flickable.StopAtBounds

            function changeZoom(factor) {
                zoom = Math.max(1, Math.min(8, zoom * factor))
                returnToBounds()
            }

            Shortcut {
                sequences: ["Ctrl++", "Ctrl+="]
                onActivated: imageView.changeZoom(1.25)
            }
            Shortcut {
                sequence: "Ctrl+-"
                onActivated: imageView.changeZoom(0.8)
            }
            Shortcut {
                sequence: "Ctrl+0"
                onActivated: imageView.zoom = 1
            }
            ScrollBar.vertical: FishUI.ScrollBar {}
            ScrollBar.horizontal: FishUI.ScrollBar {}

            Image {
                id: picture
                objectName: "previewImage"
                width: imageView.contentWidth
                height: imageView.contentHeight
                source: file.url || ""
                sourceSize.width: Math.min(4096, Math.ceil(width * preview.screen.devicePixelRatio))
                sourceSize.height: Math.min(4096, Math.ceil(height * preview.screen.devicePixelRatio))
                asynchronous: true
                retainWhileLoading: true
                autoTransform: true
                fillMode: Image.PreserveAspectFit
            }
            FishUI.Label {
                anchors.centerIn: parent
                visible: picture.status === Image.Error
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
                source: "image://fileicon/" + (file.icon || "application-octet-stream")
                fillMode: Image.PreserveAspectFit
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 6

                FishUI.Label {
                    objectName: "previewFileName"
                    Layout.fillWidth: true
                    text: file.name || ""
                    textFormat: Text.PlainText
                    wrapMode: Text.NoWrap
                    maximumLineCount: 1
                    elide: Text.ElideMiddle
                    font.bold: true
                    font.pixelSize: 16
                }
                FishUI.Label {
                    Layout.fillWidth: true
                    text: file.folder
                          ? qsTr("%1, %2").arg(file.size || "").arg(file.itemCount === 1
                              ? qsTr("1 item") : qsTr("%1 items").arg(file.itemCount || 0))
                          : file.size || ""
                }
            }
        }
    }
}
