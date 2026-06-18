import QtQuick
import ArchipelagoCore

Canvas {
    id: root

    property real brightness: 0
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
    onBrightnessChanged: requestPaint()
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
        const level = Math.max(0, Math.min(1, brightness))
        const low = level < 0.3
        const medium = level >= 0.3 && level < 0.7
        const rayCount = low ? 4 : (medium ? 6 : 8)
        const radius = w * 0.18

        ctx.strokeStyle = activeColor
        ctx.fillStyle = activeColor
        ctx.beginPath()
        ctx.arc(w * 0.5, h * 0.5, radius, 0, Math.PI * 2)
        if (low) {
            ctx.stroke()
        } else if (medium) {
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(w * 0.5, h * 0.5)
            ctx.arc(w * 0.5, h * 0.5, radius - 1, -Math.PI * 0.5, Math.PI * 0.5)
            ctx.closePath()
            ctx.fill()
        } else {
            ctx.fill()
        }

        for (let index = 0; index < rayCount; ++index) {
            const angle = (Math.PI * 2 * index) / rayCount
            const inner = w * 0.32
            const outer = w * (0.4 + level * 0.08)
            line(ctx,
                 w * 0.5 + Math.cos(angle) * inner,
                 h * 0.5 + Math.sin(angle) * inner,
                 w * 0.5 + Math.cos(angle) * outer,
                 h * 0.5 + Math.sin(angle) * outer,
                 activeColor)
        }
    }
}
