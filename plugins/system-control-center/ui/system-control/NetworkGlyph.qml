import QtQuick
import ArchipelagoCore

Canvas {
    id: root

    property bool wiredConnected: false
    property bool wifiEnabled: false
    property bool wifiConnected: false
    property int signalStrength: 0
    property int searchPhase: 0
    property color activeColor: StyleTokens.textPrimary
    property color inactiveColor: StyleTokens.textSecondary

    function line(ctx, x1, y1, x2, y2, stroke) {
        ctx.beginPath()
        ctx.strokeStyle = stroke
        ctx.moveTo(x1, y1)
        ctx.lineTo(x2, y2)
        ctx.stroke()
    }

    function drawWifiArc(ctx, radius, active) {
        ctx.beginPath()
        ctx.strokeStyle = active ? activeColor : inactiveColor
        ctx.arc(width * 0.5, height * 0.78, radius, Math.PI * 1.12, Math.PI * 1.88)
        ctx.stroke()
    }

    antialiasing: true
    onWiredConnectedChanged: requestPaint()
    onWifiEnabledChanged: requestPaint()
    onWifiConnectedChanged: requestPaint()
    onSignalStrengthChanged: requestPaint()
    onSearchPhaseChanged: requestPaint()
    onActiveColorChanged: requestPaint()
    onInactiveColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.lineWidth = Math.max(1.4, Math.min(width, height) * 0.1)
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.fillStyle = activeColor
        ctx.strokeStyle = activeColor

        const w = width
        const h = height

        if (wiredConnected) {
            line(ctx, w * 0.18, h * 0.28, w * 0.48, h * 0.28, activeColor)
            line(ctx, w * 0.48, h * 0.28, w * 0.48, h * 0.18, activeColor)
            line(ctx, w * 0.48, h * 0.28, w * 0.48, h * 0.38, activeColor)
            line(ctx, w * 0.48, h * 0.28, w * 0.76, h * 0.28, activeColor)
            line(ctx, w * 0.3, h * 0.28, w * 0.3, h * 0.62, activeColor)
            line(ctx, w * 0.44, h * 0.28, w * 0.44, h * 0.56, activeColor)
            line(ctx, w * 0.58, h * 0.28, w * 0.58, h * 0.66, activeColor)
            line(ctx, w * 0.72, h * 0.28, w * 0.72, h * 0.5, activeColor)
            return
        }

        const level = Math.max(0, Math.min(4, Math.round(signalStrength / 25)))
        const searching = wifiEnabled && !wifiConnected
        const dotColor = !wifiEnabled ? inactiveColor : activeColor

        ctx.beginPath()
        ctx.fillStyle = dotColor
        ctx.arc(w * 0.5, h * 0.8, w * 0.06, 0, Math.PI * 2)
        ctx.fill()

        if (searching) {
            drawWifiArc(ctx, w * 0.16, searchPhase >= 0)
            drawWifiArc(ctx, w * 0.3, searchPhase >= 1)
            drawWifiArc(ctx, w * 0.44, searchPhase >= 2)
        } else {
            drawWifiArc(ctx, w * 0.16, level >= 1 && wifiEnabled)
            drawWifiArc(ctx, w * 0.3, level >= 2 && wifiEnabled)
            drawWifiArc(ctx, w * 0.44, level >= 3 && wifiEnabled)
        }

        if (!wifiEnabled) {
            line(ctx, w * 0.16, h * 0.86, w * 0.84, h * 0.16, inactiveColor)
        }
    }
}
