import QtQuick
import ArchipelagoCore

Canvas {
    id: root

    property string icon: "bell"
    property color strokeColor: StyleTokens.textPrimary
    property color accentColor: StyleTokens.accent
    property real lineWidth: 1.8

    onIconChanged: requestPaint()
    onStrokeColorChanged: requestPaint()
    onAccentColorChanged: requestPaint()
    onLineWidthChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    function setup(ctx) {
        ctx.clearRect(0, 0, width, height)
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.lineWidth = root.lineWidth
        ctx.strokeStyle = root.strokeColor
        ctx.fillStyle = root.strokeColor
    }

    antialiasing: true

    onPaint: {
        const ctx = getContext("2d")
        setup(ctx)
        const w = width
        const h = height
        const cx = w / 2
        const cy = h / 2

        if (icon === "message") {
            const rx = Math.max(3, w * 0.12)
            const x = w * 0.16
            const y = h * 0.2
            const bw = w * 0.68
            const bh = h * 0.52
            ctx.beginPath()
            ctx.moveTo(x + rx, y)
            ctx.lineTo(x + bw - rx, y)
            ctx.quadraticCurveTo(x + bw, y, x + bw, y + rx)
            ctx.lineTo(x + bw, y + bh - rx)
            ctx.quadraticCurveTo(x + bw, y + bh, x + bw - rx, y + bh)
            ctx.lineTo(x + rx, y + bh)
            ctx.quadraticCurveTo(x, y + bh, x, y + bh - rx)
            ctx.lineTo(x, y + rx)
            ctx.quadraticCurveTo(x, y, x + rx, y)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(w * 0.34, h * 0.72)
            ctx.lineTo(w * 0.26, h * 0.86)
            ctx.lineTo(w * 0.48, h * 0.72)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(w * 0.30, h * 0.38)
            ctx.lineTo(w * 0.70, h * 0.38)
            ctx.moveTo(w * 0.30, h * 0.54)
            ctx.lineTo(w * 0.58, h * 0.54)
            ctx.stroke()
            return
        }

        if (icon === "low") {
            ctx.beginPath()
            ctx.arc(cx + w * 0.04, cy - h * 0.02, w * 0.30, Math.PI * 0.30, Math.PI * 1.68, false)
            ctx.arc(cx - w * 0.07, cy - h * 0.10, w * 0.25, Math.PI * 1.68, Math.PI * 0.30, true)
            ctx.stroke()
            ctx.fillStyle = root.accentColor
            ctx.beginPath()
            ctx.arc(w * 0.72, h * 0.28, Math.max(1.5, w * 0.06), 0, Math.PI * 2)
            ctx.fill()
            return
        }

        if (icon === "dnd") {
            ctx.beginPath()
            ctx.arc(cx, cy, w * 0.32, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(w * 0.28, h * 0.72)
            ctx.lineTo(w * 0.72, h * 0.28)
            ctx.stroke()
            return
        }

        ctx.beginPath()
        ctx.moveTo(w * 0.30, h * 0.55)
        ctx.quadraticCurveTo(w * 0.31, h * 0.30, cx, h * 0.25)
        ctx.quadraticCurveTo(w * 0.69, h * 0.30, w * 0.70, h * 0.55)
        ctx.lineTo(w * 0.78, h * 0.68)
        ctx.lineTo(w * 0.22, h * 0.68)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(w * 0.43, h * 0.77)
        ctx.quadraticCurveTo(cx, h * 0.86, w * 0.57, h * 0.77)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(cx, h * 0.18)
        ctx.lineTo(cx, h * 0.12)
        ctx.stroke()
    }
}
