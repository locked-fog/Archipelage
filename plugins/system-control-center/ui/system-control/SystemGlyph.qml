import QtQuick
import ArchipelagoCore

Canvas {
    id: root

    property string icon: "wifi"
    property color color: StyleTokens.textPrimary
    property real strokeWidth: Math.max(1.4, Math.min(width, height) * 0.1)

    function triangle(ctx, x1, y1, x2, y2, x3, y3) {
        ctx.beginPath()
        ctx.moveTo(x1, y1)
        ctx.lineTo(x2, y2)
        ctx.lineTo(x3, y3)
        ctx.closePath()
        ctx.fill()
    }

    function line(ctx, x1, y1, x2, y2) {
        ctx.beginPath()
        ctx.moveTo(x1, y1)
        ctx.lineTo(x2, y2)
        ctx.stroke()
    }

    antialiasing: true
    onIconChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.fillStyle = color
        ctx.strokeStyle = color
        ctx.lineWidth = strokeWidth
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        const w = width
        const h = height

        if (icon === "wifi" || icon === "wifi-off") {
            ctx.beginPath()
            ctx.arc(w * 0.5, h * 0.74, w * 0.06, 0, Math.PI * 2)
            ctx.fill()
            ctx.beginPath()
            ctx.arc(w * 0.5, h * 0.72, w * 0.18, Math.PI * 1.18, Math.PI * 1.82)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(w * 0.5, h * 0.74, w * 0.33, Math.PI * 1.14, Math.PI * 1.86)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(w * 0.5, h * 0.76, w * 0.46, Math.PI * 1.1, Math.PI * 1.9)
            ctx.stroke()
            if (icon === "wifi-off") {
                line(ctx, w * 0.18, h * 0.82, w * 0.82, h * 0.18)
            }
        } else if (icon === "bluetooth") {
            line(ctx, w * 0.5, h * 0.08, w * 0.5, h * 0.92)
            line(ctx, w * 0.5, h * 0.08, w * 0.78, h * 0.32)
            line(ctx, w * 0.5, h * 0.5, w * 0.78, h * 0.32)
            line(ctx, w * 0.5, h * 0.5, w * 0.78, h * 0.68)
            line(ctx, w * 0.5, h * 0.92, w * 0.78, h * 0.68)
            line(ctx, w * 0.5, h * 0.5, w * 0.24, h * 0.28)
            line(ctx, w * 0.5, h * 0.5, w * 0.24, h * 0.72)
        } else if (icon === "volume" || icon === "volume-muted") {
            ctx.beginPath()
            ctx.moveTo(w * 0.12, h * 0.42)
            ctx.lineTo(w * 0.3, h * 0.42)
            ctx.lineTo(w * 0.52, h * 0.24)
            ctx.lineTo(w * 0.52, h * 0.76)
            ctx.lineTo(w * 0.3, h * 0.58)
            ctx.lineTo(w * 0.12, h * 0.58)
            ctx.closePath()
            ctx.fill()
            if (icon === "volume") {
                ctx.beginPath()
                ctx.arc(w * 0.56, h * 0.5, w * 0.2, -0.72, 0.72)
                ctx.stroke()
            } else {
                line(ctx, w * 0.68, h * 0.3, w * 0.9, h * 0.7)
                line(ctx, w * 0.9, h * 0.3, w * 0.68, h * 0.7)
            }
        } else if (icon === "sun") {
            ctx.beginPath()
            ctx.arc(w * 0.5, h * 0.5, w * 0.2, 0, Math.PI * 2)
            ctx.stroke()
            for (let index = 0; index < 8; ++index) {
                const angle = (Math.PI * 2 * index) / 8
                const x1 = w * 0.5 + Math.cos(angle) * w * 0.32
                const y1 = h * 0.5 + Math.sin(angle) * h * 0.32
                const x2 = w * 0.5 + Math.cos(angle) * w * 0.46
                const y2 = h * 0.5 + Math.sin(angle) * h * 0.46
                line(ctx, x1, y1, x2, y2)
            }
        } else if (icon === "power") {
            ctx.beginPath()
            ctx.arc(w * 0.5, h * 0.54, w * 0.34, Math.PI * 0.28, Math.PI * 1.72)
            ctx.stroke()
            line(ctx, w * 0.5, h * 0.06, w * 0.5, h * 0.46)
        } else if (icon === "speaker") {
            ctx.beginPath()
            ctx.moveTo(w * 0.14, h * 0.48)
            ctx.lineTo(w * 0.28, h * 0.48)
            ctx.lineTo(w * 0.44, h * 0.32)
            ctx.lineTo(w * 0.44, h * 0.68)
            ctx.lineTo(w * 0.28, h * 0.52)
            ctx.lineTo(w * 0.14, h * 0.52)
            ctx.closePath()
            ctx.fill()
            line(ctx, w * 0.56, h * 0.3, w * 0.84, h * 0.3)
            line(ctx, w * 0.56, h * 0.5, w * 0.84, h * 0.5)
            line(ctx, w * 0.56, h * 0.7, w * 0.84, h * 0.7)
        } else if (icon === "bolt") {
            ctx.beginPath()
            ctx.moveTo(w * 0.56, h * 0.06)
            ctx.lineTo(w * 0.28, h * 0.52)
            ctx.lineTo(w * 0.5, h * 0.52)
            ctx.lineTo(w * 0.42, h * 0.94)
            ctx.lineTo(w * 0.74, h * 0.42)
            ctx.lineTo(w * 0.52, h * 0.42)
            ctx.closePath()
            ctx.fill()
        } else {
            triangle(ctx, w * 0.24, h * 0.1, w * 0.82, h * 0.5, w * 0.24, h * 0.9)
        }
    }
}
