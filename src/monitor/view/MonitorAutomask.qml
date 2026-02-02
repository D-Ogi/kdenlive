/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Effects
import QtQuick 2.15

import org.kde.kdenlive as K

Item {
    id: root
    objectName: "root"

    SystemPalette { id: activePalette }

    // default size, but scalable by user
    height: 300; width: 400
    property string markerText
    property int itemType: 0
    property point profile: controller.profile
    property int displayFrame: controller.position
    property double zoom
    property point center
    property double scalex
    property double scaley
    property bool captureRightClick: false
    // Zoombar properties
    // The start position of the zoomed area, between 0 and 1
    property double zoomStart: 0
    // The zoom factor (between 0 and 1). 0.5 means 2x zoom
    property double zoomFactor: 1
    // The pixel height of zoom bar, used to offset markers info
    property int zoomOffset: 0
    property bool showZoomBar: false
    property double offsetx : 0
    property double offsety : 0
    property bool dropped: false
    property string fps: '-'
    property bool showMarkers: false
    property bool showTimecode: false
    property bool showFps: false
    property bool showSafezone: false
    // Display hover audio thumbnails overlay
    property bool showAudiothumb: false
    property bool showClipJobs: false
    property bool showToolbar: false
    property string clipName: controller.clipName
    property real baseUnit: fontMetrics.font.pixelSize * 0.8
    property int duration: 300
    property int mouseRulerPos: 0
    property double frameSize: 10
    property double timeScale: 1
    property var centerPoints: []
    property var centerPointsTypes: []
    property var boxCoords: [0, 0, 0, 0]
    // The frame positions that have points defined
    property var keyframes: []
    property int maskStart: -1
    property int maskEnd: -1
    property int overlayType: controller.overlayType
    property bool isClipMonitor: true
    property int dragType: 0
    property string baseThumbPath
    property int overlayMargin: 0
    property int maskMode: controller.maskMode
    // Brush tool properties
    property int brushSize: 20
    property bool brushMode: true // true = brush is default tool
    property bool isBrushing: false
    property var currentStrokePoints: []
    property bool currentStrokeIsExclude: false
    Component.onCompleted: {
        controller.rulerHeight = root.zoomOffset
    }

    onDisplayFrameChanged: {
        if (root.maskStart > -1 && (root.displayFrame < root.maskStart || root.displayFrame > root.maskEnd)) {
            outsideLabel.visible = true
        } else if (outsideLabel.visible) {
            outsideLabel.visible = false
        }
        // Clear stale brush canvas on seek (but not mid-stroke)
        if (!root.isBrushing) {
            root.clearBrushCanvas()
        }
    }

    onMaskModeChanged: {
        if (root.maskMode == K.MaskModeType.MaskPreview) {
            generateLabel.visible = false
        }
        // Reset brush state if mode changes mid-stroke
        if (frameArea.isBrushEvent) {
            frameArea.isBrushEvent = false
            root.isBrushing = false
            root.clearBrushCanvas()
        }
        updateKeyBindingTooltip()
    }

    onBrushModeChanged: {
        updateKeyBindingTooltip()
    }

    function updateKeyBindingTooltip() {
        if (!frameArea.containsMouse) {
            return
        }
        if (root.maskMode === K.MaskModeType.MaskPreview) {
            controller.setWidgetKeyBinding();
        } else if (root.brushMode) {
            controller.setWidgetKeyBinding(xi18nc("@info:whatsthis","<shortcut>Drag</shortcut> to paint a brush stroke, <shortcut>Alt+drag</shortcut> to exclude, <shortcut>Shift+click</shortcut> to add a point, <shortcut>Ctrl+drag</shortcut> to draw a box."));
        } else {
            controller.setWidgetKeyBinding(xi18nc("@info:whatsthis","<shortcut>Click</shortcut> to add a point, <shortcut>Alt+click</shortcut> to exclude, <shortcut>Ctrl+drag</shortcut> to draw a box, <shortcut>Shift+click</shortcut> to extend."));
        }
    }

    function updatePoints(keyframes, types, points) {
        root.keyframes = keyframes
        root.centerPointsTypes = types
        root.centerPoints = points
    }

    function updateRect(keyframes, box) {
        root.keyframes = keyframes
        if (box.length == 4 && box[2] > 0) {
            root.boxCoords = box
        } else {
            root.boxCoords = [0, 0, 0, 0]
        }
    }

    function updateClickCapture() {
        root.captureRightClick = false
    }

    function clearBrushCanvas() {
        root.currentStrokePoints = []
        brushCanvas.requestPaint()
    }
    
    FontMetrics {
        id: fontMetrics
        font: fixedFont
    }

    Timer {
        id: thumbTimer
        interval: 3000; running: false;
    }

    signal moveControlPoint(int index, real x, real y)
    signal addControlPoint(real x, real y, bool extend, bool exclude)
    signal addControlRect(real x, real y, real width, real height, bool extend)
    signal addControlStroke(var points, bool isExclude)
    signal generateMask()
    signal exitMaskPreview()

    onDurationChanged: {
        clipMonitorRuler.updateRuler()
    }
    onWidthChanged: {
        clipMonitorRuler.updateRuler()
    }
    
    onZoomOffsetChanged: {
        controller.rulerHeight = root.zoomOffset
    }
    
    onHeightChanged: {
        controller.rulerHeight = root.zoomOffset
    }

    function updatePalette() {
        clipMonitorRuler.forceRepaint()
    }

    function switchOverlay() {
        if (controller.overlayType >= 5) {
            controller.overlayType = 0
        } else {
            controller.overlayType = controller.overlayType + 1;
        }
        root.overlayType = controller.overlayType
    }

    Item {
        id: monitorframe
        height: root.height - controller.rulerHeight
        width: root.width
        Item {
            id: frame
            objectName: "referenceframe"
            width: root.profile.x * root.scalex
            height: root.profile.y * root.scaley
            x: root.center.x - width / 2 - root.offsetx;
            y: root.center.y - height / 2 - root.offsety;

            K.MonitorOverlay {
                anchors.fill: frame
                color: K.KdenliveSettings.overlayColor
                overlayType: root.overlayType
            }
            K.MonitorSafeZone {
                id: safeZone
                anchors.fill: frame
                color: K.KdenliveSettings.safeColor
                showSafeZone: controller.showSafezone
            }

            MouseArea {
                id: frameArea
                hoverEnabled: true
                anchors.fill: frame
                property bool shiftClick: false
                property bool ctrlClick: false
                property bool altClick: false
                property bool handleEvent: false
                property bool isPanEvent: false
                property bool isRectEvent: false
                property bool isBrushEvent: false
                property real clickPointX: 0
                property real clickPointY: 0
                property real xPos: 0
                property real yPos: 0
                onPressed: mouse => {
                    if (root.maskMode != K.MaskModeType.MaskPreview) {
                        shiftClick = mouse.modifiers & Qt.ShiftModifier
                        ctrlClick = mouse.modifiers & Qt.ControlModifier
                        altClick = mouse.modifiers & Qt.AltModifier
                        clickPointX = mouseX
                        clickPointY = mouseY
                        selectionRect.x = mouseX
                        selectionRect.y = mouseY
                        isRectEvent = false
                        isPanEvent = false
                        isBrushEvent = false
                        // Start brush stroke if in brush mode and no Shift (point) or Ctrl (box)
                        if (root.brushMode && !shiftClick && !ctrlClick) {
                            isBrushEvent = true
                            root.isBrushing = true
                            root.currentStrokeIsExclude = altClick
                            root.currentStrokePoints = [Qt.point(mouseX / frame.width, mouseY / frame.height)]
                            brushCanvas.lastX = mouseX
                            brushCanvas.lastY = mouseY
                            brushCanvas.requestPaint()
                        }
                    } else {
                        mouse.accepted = false;
                    }
                    handleEvent = mouse.button == Qt.LeftButton
                }
                onPositionChanged: mouse => {
                    if (!pressed) return
                    if (isBrushEvent) {
                        // Accumulate brush stroke points
                        var nx = mouseX / frame.width
                        var ny = mouseY / frame.height
                        var pts = root.currentStrokePoints
                        pts.push(Qt.point(nx, ny))
                        root.currentStrokePoints = pts
                        brushCanvas.lastX = mouseX
                        brushCanvas.lastY = mouseY
                        brushCanvas.requestPaint()
                        return
                    }
                    if (!isPanEvent && root.maskMode !== K.MaskModeType.MaskPreview && ctrlClick && (Math.abs(mouseX - selectionRect.x) + Math.abs(mouseY - selectionRect.y) > Qt.styleHints.startDragDistance)) {
                        isRectEvent = true
                        selectionRect.visible = true
                        mouse.accepted = true;
                    }
                    if (isRectEvent) {
                        selectionRect.width = Math.abs(mouseX - clickPointX)
                        if (mouseX < clickPointX) {
                            selectionRect.x = mouseX
                        }
                        selectionRect.height = Math.abs(mouseY - clickPointY)
                        if (mouseY < clickPointY) {
                            selectionRect.y = mouseY
                        }
                    }
                }
                onReleased: mouse => {
                    if (root.maskMode === K.MaskModeType.MaskPreview) {
                        mouse.accepted = false
                        handleEvent = false
                        return;
                    }
                    root.captureRightClick = false
                    selectionRect.visible = false
                    if (handleEvent) {
                        if (isBrushEvent && root.currentStrokePoints.length > 0) {
                            // Emit brush stroke
                            root.addControlStroke(root.currentStrokePoints, root.currentStrokeIsExclude)
                            root.isBrushing = false
                            generateLabel.visible = true
                        } else if (isRectEvent) {
                            xPos = selectionRect.x / frame.width
                            yPos = selectionRect.y / frame.height
                            addControlRect(xPos, yPos, selectionRect.width / frame.width, selectionRect.height / frame.height, shiftClick)
                            generateLabel.visible = true
                        } else if (!isPanEvent && !isBrushEvent) {
                            // Single point selection (Shift+click)
                            xPos = mouse.x / frame.width
                            yPos = mouse.y / frame.height
                            addControlPoint(xPos, yPos, shiftClick, altClick)
                            generateLabel.visible = true
                        }
                    }
                    handleEvent = false
                    isBrushEvent = false
                }
                onEntered: {
                    root.updateKeyBindingTooltip()
                }
                onExited: {
                    controller.setWidgetKeyBinding();
                }
                Rectangle {
                    id: selectionRect
                    color: '#66ffffff'
                    border.color: 'red'
                    border.width: 1
                }
            }
            // Brush stroke canvas overlay
            Canvas {
                id: brushCanvas
                anchors.fill: frame
                visible: root.maskMode != K.MaskModeType.MaskPreview
                property real lastX: 0
                property real lastY: 0
                onPaint: {
                    var ctx = getContext("2d")
                    if (root.currentStrokePoints.length < 2) {
                        ctx.reset()
                        return
                    }
                    ctx.reset()
                    ctx.lineWidth = root.brushSize * root.scalex
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.strokeStyle = root.currentStrokeIsExclude ? "rgba(200, 0, 0, 0.5)" : "rgba(0, 180, 0, 0.5)"
                    ctx.beginPath()
                    var pts = root.currentStrokePoints
                    ctx.moveTo(pts[0].x * frame.width, pts[0].y * frame.height)
                    for (var i = 1; i < pts.length; i++) {
                        ctx.lineTo(pts[i].x * frame.width, pts[i].y * frame.height)
                    }
                    ctx.stroke()
                }
            }
            Image {
                id: maskPreview
                anchors.fill: frame
                source: root.maskMode != K.MaskModeType.MaskPreview ? controller.previewOverlay : ''
                asynchronous: true
                opacity: controller.maskOpacity / 100
                visible: root.maskMode != K.MaskModeType.MaskPreview
                onSourceChanged: {
                    generateLabel.visible = false
                    root.clearBrushCanvas()
                    if (opacity == 0 && source != '') {
                        // Update opacity to ensure we see something
                        controller.maskOpacity = 50
                    }
                }
            }
            Item {
                anchors.fill: frame
                Repeater {
                    model: root.centerPoints.length
                    delegate:
                    Rectangle {
                        id: kfrPoint
                        required property int index
                        property bool isNegative: root.centerPointsTypes[index] == 0
                        x: root.centerPoints[index].x * frame.width - width / 2
                        y: root.centerPoints[index].y * frame.height - height / 2
                        color: isNegative ? "#FF990000" : "#FF006600"
                        height: baseUnit * 1.5
                        width: height
                        radius: 180
                        border.width: 2
                        border.color: "white"
                        Rectangle {
                            anchors.fill: kfrPoint
                            anchors.leftMargin: kfrPoint.width / 4
                            anchors.rightMargin: kfrPoint.width / 4
                            anchors.topMargin: kfrPoint.height / 2 - 1
                            anchors.bottomMargin: kfrPoint.height / 2 - 1
                            color: "#FFFFFF"
                        }
                        Rectangle {
                            visible: !kfrPoint.isNegative
                            anchors.fill: kfrPoint
                            anchors.leftMargin: kfrPoint.width / 2 - 1
                            anchors.rightMargin: kfrPoint.width / 2 - 1
                            anchors.topMargin: kfrPoint.height / 4
                            anchors.bottomMargin: kfrPoint.height / 4
                            color: "#FFFFFF"
                        }
                        MouseArea {
                            anchors.fill: kfrPoint
                            cursorShape: Qt.PointingHandCursor
                            drag.target: kfrPoint
                            drag.smoothed: false
                            onPressed: mouse => {
                                root.captureRightClick = true
                                mouse.accepted = true
                            }
                            onReleased: mouse => {
                                mouse.accepted = true
                                root.captureRightClick = false
                                var positionInFrame = mapToItem(frame, mouse.x, mouse.y)
                                moveControlPoint(index, positionInFrame.x / frame.width, positionInFrame.y / frame.height)
                                generateLabel.visible = true
                            }

                        }
                    }
                }
                Rectangle {
                    id: frameBox
                    color: '#33ffffff'
                    border.color: '#ff0000'
                    border.width: 1
                    x: root.boxCoords[0] * frame.width
                    y: root.boxCoords[1] * frame.height
                    width: root.boxCoords[2] * frame.width
                    height: root.boxCoords[3] * frame.height
                    visible: root.boxCoords[2] > 0
                }
            }
        }
    }
    Label {
        id: generateLabel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.topMargin: 10
        padding: 5
        text: keyframes.length == 0 ? i18n("Select an object in the image first") : root.maskMode != K.MaskModeType.MaskPreview ? i18n("Generating image mask") : i18n("Generating video mask")
        visible: false
        background: Rectangle {
            color: keyframes.length == 0 ? "darkred" : Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.8)
            radius: 5
        }
        SequentialAnimation on opacity {
            id: pulseAnim
            running: generateLabel.visible && root.keyframes.length > 0
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 0.4; duration: 800; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 0.4; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
        }
    }
    Label {
        id: infoLabel
        anchors.centerIn: parent
        padding: 5
        text: root.maskMode === K.MaskModeType.MaskPreview ? i18n("Previewing video mask") : root.brushMode ? i18n("Paint a brush stroke to select an object.\nAlt+drag to exclude a zone.\nShift+click to add a point, Ctrl+drag for a box.") : i18n("Click to add a point, Alt+click to exclude.\nCtrl+drag to draw a box.\nShift+click to extend selection.")
        visible: root.centerPoints.length == 0 && !frameBox.visible && !frameArea.containsMouse && !generateLabel.visible && !outsideLabel.visible && keyframes.length == 0
        background: Rectangle {
            color: Qt.rgba(activePalette.window.r, activePalette.window.g, activePalette.window.b, 0.8)
            radius: 5
        }
    }
    Label {
        id: outsideLabel
        anchors.centerIn: parent
        padding: 5
        text: i18n("You are outside of the time zone defined\nfor the mask and cannot add keyframes.\n\n\n")
        visible: false
        color: 'white'
        background: Rectangle {
            color: 'darkred'
            radius: 5
        }
        ToolButton {
            anchors.bottom: outsideLabel.bottom
            anchors.left: outsideLabel.left
            text: i18n("Go to mask start")
            onPressed: () =>{
                root.captureRightClick = true
            }
            onReleased: () => {
                root.updateClickCapture()
            }
            onClicked: controller.position = root.maskStart
        }
        ToolButton {
            anchors.bottom: outsideLabel.bottom
            anchors.right: outsideLabel.right
            text: i18n("Go to mask end")
            onPressed: () =>{
                root.captureRightClick = true
            }
            onReleased: () => {
                root.updateClickCapture()
            }
            onClicked: controller.position = root.maskEnd
        }
    }
    MaskToolBar {
        id: sceneToolBar
        anchors {
            right: parent.right
            top: parent.top
            topMargin: 4
            rightMargin: 4
            leftMargin: 4
        }
    }
    Timer {
        id: firstTimer
        interval: 3000; running: true; repeat: false
    }
    Rectangle {
        id: monitoredge
        anchors.fill: monitorframe
        color: 'transparent'
        border.width: firstTimer.running ? 4 : 1
        border.color: 'darkred'
        Label {
            anchors.horizontalCenter: monitoredge.horizontalCenter
            text: i18n('Mask Mode')
            padding: 5
            background: Rectangle {
                color: 'darkred'
            }
            visible: firstTimer.running
        }
    }
    // Mask generation progress bar (span indicator)
    Rectangle {
        id: maskProgressBar
        anchors {
            left: root.left
            right: root.right
            bottom: clipMonitorRuler.top
        }
        height: 3
        color: "transparent"
        visible: controller.maskProgress >= 0
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.max(0, Math.min(controller.maskProgress, 100)) / 100
            color: "#44bb44"
            Behavior on width {
                NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
            }
        }
    }
    MonitorRuler {
        id: clipMonitorRuler
        anchors {
            left: root.left
            right: root.right
            bottom: root.bottom
        }
        visible: root.duration > 0
        height: controller.rulerHeight
        Repeater {
            model:root.keyframes
            anchors.fill: parent
            Rectangle {
                id: marker
                property int kf: modelData + root.maskStart
                anchors.bottom: clipMonitorRuler.bottom
                color: 'red'
                width: clipMonitorRuler.height / 2
                height: width
                radius: width
                x: kf * root.timeScale - (frame.width/root.zoomFactor * root.zoomStart) - width / 2
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onPressed: mouse =>{
                        root.captureRightClick = true
                        mouse.accepted = true
                    }
                    onClicked: {
                        controller.position = marker.kf
                    }
                    onReleased: mouse => {
                        root.updateClickCapture()
                    }
                }
            }
        }
    }
}
