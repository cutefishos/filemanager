import QtQuick
import QtQuick.Effects
import FishUI 1.0 as FishUI

// Frosted backdrop for the bars that float over the top of a folder view: the
// content scrolled beneath them, blurred, under a translucent tint. At rest the
// bars and the content read as one surface; the material fades in once content
// scrolls under them.
Item {
    id: control
    objectName: "headerBackdrop"

    required property Flickable view
    property bool separatorVisible: true

    // Content below the backdrop feeds the blur, so its bottom edge does not
    // fade out the way a blur of a cropped source would.
    readonly property real blurPadding: 32
    readonly property real fadeDistance: 16
    readonly property real restContentY: view.originY - view.topMargin

    clip: true
    opacity: Math.max(0, Math.min(1, (view.contentY - restContentY) / fadeDistance))
    // Also stops the live source from rendering while nothing is beneath.
    visible: opacity > 0

    // The view's own content is still drawn beneath the backdrop; without an
    // opaque base it would show through the tint unblurred.
    Rectangle {
        anchors.fill: parent
        color: FishUI.Theme.backgroundColor
    }

    ShaderEffectSource {
        id: _source
        // The content item alone: anything else floating over the view would
        // otherwise be blurred into its own backdrop.
        sourceItem: control.view.contentItem
        // Flickable moves its content item by (-contentX, -contentY).
        sourceRect: Qt.rect(control.view.contentX + control.x, control.view.contentY + control.y,
                            control.width, control.height + control.blurPadding)
        width: control.width
        height: control.height + control.blurPadding
        visible: false
    }

    MultiEffect {
        width: _source.width
        height: _source.height
        source: _source
        autoPaddingEnabled: false
        blurEnabled: true
        blurMax: 32
        blur: 1.0
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(FishUI.Theme.backgroundColor.r,
                       FishUI.Theme.backgroundColor.g,
                       FishUI.Theme.backgroundColor.b,
                       0.72)
    }

    // A 0.5px hairline. Below 2x that is less than a device pixel, so it is drawn
    // one device pixel wide with its alpha scaled to the same weight.
    Rectangle {
        readonly property real devicePixels: Math.max(1, Math.round(0.5 * FishUI.Dpi.ratio))

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: devicePixels / FishUI.Dpi.ratio
        visible: control.separatorVisible
        color: FishUI.Theme.textColor
        opacity: (FishUI.Theme.darkMode ? 0.12 : 0.08) * 0.5 * FishUI.Dpi.ratio / devicePixels
    }
}
