// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI 1.0 as FishUI

// An image fitted to the view, never enlarged past its own pixels until the
// user zooms. Zoom 1 is one image pixel per device pixel.
Item {
    id: viewer
    objectName: "previewImageView"
    clip: true

    // The file description, plus `prepared`: the fitted decode size the window
    // already has in the pixmap cache. Set as one value, so it loads once.
    property var image: ({})
    readonly property url source: image.url || ""
    // Pixels as displayed, after the EXIF orientation; invalid when unknown.
    readonly property size pixelSize: Qt.size(image.width || 0, image.height || 0)
    readonly property bool vector: !!image.vector
    readonly property bool animated: !!image.animated

    property real ratio: FishUI.Dpi.ratio > 0 ? FishUI.Dpi.ratio : 1
    readonly property bool knownSize: pixelSize.width > 0 && pixelSize.height > 0
    readonly property real naturalWidth: knownSize ? pixelSize.width / ratio : Math.max(1, width)
    readonly property real naturalHeight: knownSize ? pixelSize.height / ratio : Math.max(1, height)
    readonly property real fitZoom: width > 0 && height > 0
                                    ? Math.min(1, width / naturalWidth, height / naturalHeight) : 1
    readonly property real maxZoom: Math.max(fitZoom, 8)
    readonly property bool zoomed: zoom > fitZoom + 0.0001
    readonly property bool loading: animated ? animatedLayer.status === Image.Loading
                                             : fitLayer.status === Image.Loading
    readonly property bool failed: animated ? animatedLayer.status === Image.Error
                                            : fitLayer.status === Image.Error

    property real zoom: 1
    property bool fitted: true

    property url fitSource
    property size fitSourceSize
    property bool detailRequested: false
    property size detailSourceSize

    property bool anchored: false
    property real anchorViewX: 0
    property real anchorViewY: 0
    property real anchorImageX: 0
    property real anchorImageY: 0

    function snap(value) {
        return Math.round(value * ratio) / ratio
    }

    // Where the frame sits in the content at a zoom: centred while smaller
    // than the view. From zoom itself, not the bindings it is still updating.
    function frameX(z) {
        return Math.max(0, (width - naturalWidth * z) / 2)
    }

    function frameY(z) {
        return Math.max(0, (height - naturalHeight * z) / 2)
    }

    function setPan(contentX, contentY) {
        flick.contentX = Math.max(0, Math.min(Math.max(0, naturalWidth * zoom - width), contentX))
        flick.contentY = Math.max(0, Math.min(Math.max(0, naturalHeight * zoom - height), contentY))
    }

    function clampPan() {
        setPan(flick.contentX, flick.contentY)
    }

    function resetZoom() {
        zoomAnimation.stop()
        anchored = false
        fitted = true
        zoom = fitZoom
        setPan(0, 0)
    }

    // Zooms keeping the image point under (viewX, viewY) where it is.
    function zoomTo(target, viewX, viewY, animate) {
        target = Math.max(fitZoom, Math.min(maxZoom, target))
        anchorImageX = Math.max(0, Math.min(naturalWidth, (viewX + flick.contentX - frameX(zoom)) / zoom))
        anchorImageY = Math.max(0, Math.min(naturalHeight, (viewY + flick.contentY - frameY(zoom)) / zoom))
        anchorViewX = viewX
        anchorViewY = viewY
        fitted = target <= fitZoom + 0.0001
        zoomAnimation.stop()
        anchored = true
        if (animate && Math.abs(target - zoom) > 0.0001) {
            zoomAnimation.from = zoom
            zoomAnimation.to = target
            zoomAnimation.start()
        } else {
            zoom = target
        }
    }

    // Steps compound on an animation's target, so quick wheel clicks add up.
    function zoomBy(factor, viewX, viewY, animate) {
        const base = zoomAnimation.running ? zoomAnimation.to : zoom
        zoomTo(base * factor, viewX, viewY, animate)
    }

    function toggleActualSize(viewX, viewY) {
        if (zoomed)
            zoomTo(fitZoom, viewX, viewY, true)
        else
            zoomTo(fitZoom < 1 ? 1 : 2, viewX, viewY, true)
    }

    // The largest texture to ask for; a 50 MP photo stays inside common GPU limits.
    function cappedSize(w, h) {
        const scale = Math.min(1, 8192 / Math.max(w, h))
        return Qt.size(Math.round(w * scale), Math.round(h * scale))
    }

    // Reads `image` itself: in its change handler the bindings derived from it
    // still hold the previous file.
    function load() {
        detailRequested = false
        resetZoom()
        const known = image.width > 0 && image.height > 0
        // The size first, so the new source is decoded once.
        fitSourceSize = image.prepared ? image.prepared
                                       : Qt.size(Math.ceil(width * ratio), Math.ceil(height * ratio))
        fitSource = image.animated ? "" : (image.url || "")
        detailSourceSize = image.vector || !known ? Qt.size(0, 0)
                                                  : cappedSize(image.width, image.height)
    }

    onImageChanged: load()
    Component.onCompleted: load()

    onFitZoomChanged: {
        if (fitted)
            resetZoom()
        else
            clampPan()
    }
    onWidthChanged: clampPan()
    onHeightChanged: clampPan()

    onZoomChanged: {
        if (anchored)
            setPan(frameX(zoom) + anchorImageX * zoom - anchorViewX,
                   frameY(zoom) + anchorImageY * zoom - anchorViewY)
        else
            clampPan()
        if (zoomed)
            detailRequested = true
        if (vector)
            vectorTimer.restart()
    }

    NumberAnimation {
        id: zoomAnimation
        target: viewer
        property: "zoom"
        duration: 200
        easing.type: Easing.OutCubic
    }

    // A vector image is drawn again at the zoom it comes to rest at.
    Timer {
        id: vectorTimer
        interval: 150
        onTriggered: {
            if (viewer.zoomed)
                viewer.detailSourceSize = viewer.cappedSize(Math.ceil(frame.width * viewer.ratio),
                                                            Math.ceil(frame.height * viewer.ratio))
        }
    }

    // Moved only by the handlers below; FishUI.WheelHandler gives touchpad
    // panning the same resampling, momentum and edges as every scroll view.
    Flickable {
        id: flick
        anchors.fill: parent
        interactive: false
        contentWidth: Math.max(width, frame.width)
        contentHeight: Math.max(height, frame.height)

        FishUI.ScrollBar.horizontal: FishUI.ScrollBar { id: horizontalBar }
        FishUI.ScrollBar.vertical: FishUI.ScrollBar { id: verticalBar }

        Item {
            id: frame
            width: viewer.naturalWidth * viewer.zoom
            height: viewer.naturalHeight * viewer.zoom
            x: viewer.snap(viewer.frameX(viewer.zoom))
            y: viewer.snap(viewer.frameY(viewer.zoom))

            Image {
                id: fitLayer
                objectName: "previewImage"
                anchors.fill: parent
                visible: !viewer.animated
                source: viewer.fitSource
                sourceSize: viewer.fitSourceSize
                // No retainWhileLoading: it keeps the old texture up when the new
                // one comes synchronously from the cache the window preloaded.
                asynchronous: true
                autoTransform: true
                smooth: true
                fillMode: Image.PreserveAspectFit
            }

            // Full resolution, decoded the first time the image is zoomed past fit.
            Image {
                id: detailLayer
                anchors.fill: parent
                visible: viewer.zoomed && status === Image.Ready && !viewer.animated
                source: viewer.detailRequested && !viewer.animated
                        && viewer.detailSourceSize.width > 0 ? viewer.source : ""
                sourceSize: viewer.detailSourceSize
                asynchronous: true
                retainWhileLoading: viewer.vector
                autoTransform: true
                cache: false
                smooth: true
                mipmap: !viewer.vector
                fillMode: Image.PreserveAspectFit
            }

            AnimatedImage {
                id: animatedLayer
                anchors.fill: parent
                visible: viewer.animated
                source: viewer.animated ? viewer.source : ""
                playing: visible && viewer.Window.window !== null && viewer.Window.window.visible
                asynchronous: true
                autoTransform: true
                cache: false
                smooth: true
                fillMode: Image.PreserveAspectFit
            }
        }
    }

    HoverHandler {
        cursorShape: panHandler.active ? Qt.ClosedHandCursor
                                       : viewer.zoomed ? Qt.OpenHandCursor : Qt.ArrowCursor
    }

    DragHandler {
        id: panHandler
        target: null
        enabled: viewer.zoomed
        property real startX: 0
        property real startY: 0
        onActiveChanged: {
            if (active) {
                zoomAnimation.complete()
                viewer.anchored = false
                startX = flick.contentX
                startY = flick.contentY
            }
        }
        onActiveTranslationChanged: {
            if (active)
                viewer.setPan(startX - activeTranslation.x, startY - activeTranslation.y)
        }
    }

    PinchHandler {
        target: null
        acceptedDevices: PointerDevice.TouchPad | PointerDevice.TouchScreen
        onScaleChanged: function(delta) {
            viewer.zoomTo(viewer.zoom * delta, centroid.position.x, centroid.position.y, false)
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: function(eventPoint) {
            viewer.toggleActualSize(eventPoint.position.x, eventPoint.position.y)
        }
    }

    // A pan along one axis shows both bars, as each bar alone shows only
    // while its own axis moves.
    Binding {
        target: horizontalBar
        property: "active"
        value: true
        when: wheelHandler.scrolling || panHandler.active
    }

    Binding {
        target: verticalBar
        property: "active"
        value: true
        when: wheelHandler.scrolling || panHandler.active
    }

    FishUI.WheelHandler {
        id: wheelHandler
        target: flick
        onWheel: function(event) {
            if (!(event.modifiers & Qt.ControlModifier)) {
                // A pan ends a running zoom, or its anchor would pull the view back.
                zoomAnimation.complete()
                viewer.anchored = false
                return
            }
            event.accepted = true
            if (event.pixelDelta.y !== 0)
                viewer.zoomBy(Math.pow(1.006, event.pixelDelta.y), event.x, event.y, false)
            else if (event.angleDelta.y !== 0)
                viewer.zoomBy(Math.pow(1.25, event.angleDelta.y / 120), event.x, event.y, true)
        }
    }

    Shortcut {
        sequences: ["Ctrl++", "Ctrl+="]
        onActivated: viewer.zoomBy(1.5, viewer.width / 2, viewer.height / 2, true)
    }
    Shortcut {
        sequence: "Ctrl+-"
        onActivated: viewer.zoomBy(1 / 1.5, viewer.width / 2, viewer.height / 2, true)
    }
    Shortcut {
        sequence: "Ctrl+0"
        onActivated: viewer.zoomTo(viewer.fitZoom, viewer.width / 2, viewer.height / 2, true)
    }
    Shortcut {
        sequence: "Ctrl+9"
        onActivated: viewer.zoomTo(1, viewer.width / 2, viewer.height / 2, true)
    }
}
