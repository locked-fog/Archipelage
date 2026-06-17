import QtQuick
import ArchipelagoCore

Canvas {
    id: root

    property real volume: 0
    property bool muted: false
    property color activeColor: StyleTokens.textPrimary
    property color inactiveColor: StyleTokens.textSecondary

    function line(ctx, x1, y1, x2, y2, stroke) {
        ctx.beginPath()
        ctx.strokeStyle = stroke
        ctx.moveTo(x1, y1)
        ctx.lineTo(x2, y2)
        ctx.stroke()
    }

    antialiasing: true
    onVolumeChanged: requestPaint()
    onMutedChanged: requestPaint()
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

        const w = width
        const h = height
        const level = muted ? 0 : Math.max(0, Math.min(3, Math.ceil(Math.max(volume, 0) * 3)))

        ctx.fillStyle = level > 0 ? activeColor : inactiveColor
        ctx.beginPath()
        ctx.moveTo(w * 0.12, h * 0.42)
        ctx.lineTo(w * 0.3, h * 0.42)
        ctx.lineTo(w * 0.5, h * 0.24)
        ctx.lineTo(w * 0.5, h * 0.76)
        ctx.lineTo(w * 0.3, h * 0.58)
        ctx.lineTo(w * 0.12, h * 0.58)
        ctx.closePath()
        ctx.fill()

        for (let index = 0; index < 3; ++index) {
            ctx.beginPath()
            ctx.strokeStyle = index < level ? activeColor : inactiveColor
            const radius = w * (0.18 + index * 0.11)
            ctx.arc(w * 0.54, h * 0.5, radius, -0.72, 0.72)
            ctx.stroke()
        }
    }
}
