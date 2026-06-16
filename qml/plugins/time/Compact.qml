import QtQuick
import ArchipelagoCore
import ArchipelagoBackend

// Compact time display. Renders HH:mm as a row of fixed-width digit
// slots, each animating a slow upward roll + cross-fade when its digit
// changes. This keeps the layout from shifting horizontally as the
// proportional time font swaps between narrower and wider glyphs, and
// replaces a hard cut with a deliberate transition.
Item {
    id: root

    property int compactLevel: 0
    property string moduleId: ""
    property var handlers: ({})
    property date now: new Date()

    // Easing curve shared with IslandCapsule and ExpandedSurface fades so
    // digit motion reads as part of the same visual language.
    readonly property var fadeCurve: [0.33, 1, 0.68, 1, 1, 1]
    // Digit roll is intentionally slow: hh:mm ticks once a minute, so
    // a 500 ms transition reads as a deliberate beat rather than a
    // reactive flicker.
    readonly property int rollDuration: 500
    readonly property int pixelSize: compactLevel >= 2 ? 14 : 15

    // Single font definition shared by every slot and the colon, so a
    // future config tweak (e.g. weight, hinting) only needs one edit.
    readonly property font timeFont: Qt.font({
        family: ArchipelagoConfig.timeFontFamily,
        pixelSize: pixelSize,
        weight: Font.DemiBold,
        fixedPitch: true
    })

    // Per-slot cell dimensions. Pinned to a pixelSize-based default so
    // the first frame already has a stable width before the font has
    // been measured; Component.onCompleted refines the values.
    property real fixedCellW: Math.max(8, Math.round(pixelSize * 0.62))
    property real lineHeight: Math.max(12, Math.round(pixelSize * 1.2))

    // Hidden text used to measure the widest glyph for the configured
    // font. Sampling every digit plus the colon gives a stable width
    // regardless of which character is currently rendered.
    Text {
        id: measurer
        visible: false
        text: "0"
    }

    function measure() {
        measurer.font = timeFont
        const sample = "0123456789:"
        let maxW = 0
        for (let i = 0; i < sample.length; ++i) {
            measurer.text = sample[i]
            if (measurer.contentWidth > maxW)
                maxW = measurer.contentWidth
        }
        if (maxW > 0)
            fixedCellW = Math.max(8, Math.ceil(maxW) + 1)
        if (measurer.contentHeight > 0)
            lineHeight = Math.max(12, Math.ceil(measurer.contentHeight) + 1)
        // Prime the slots with the current digits so the first visible
        // frame is correct, before the timer fires for the first time.
        syncDigits()
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }

    onNowChanged: syncDigits()

    function syncDigits() {
        const h = now.getHours()
        const m = now.getMinutes()
        const hh = (h < 10 ? "0" : "") + h
        const mm = (m < 10 ? "0" : "") + m
        slotH1.setDigit(hh[0])
        slotH2.setDigit(hh[1])
        slotM1.setDigit(mm[0])
        slotM2.setDigit(mm[1])
    }

    Component.onCompleted: measure()

    Row {
        anchors.centerIn: parent
        spacing: 7

        Rectangle {
            width: 7
            height: 7
            radius: 4
            color: StyleTokens.danger
            visible: SystemServices.screenRecordingActive
            anchors.verticalCenter: parent.verticalCenter
        }

        Row {
            spacing: 0
            anchors.verticalCenter: parent.verticalCenter

            DigitSlot { id: slotH1 }
            DigitSlot { id: slotH2 }

            Text {
                text: ":"
                color: StyleTokens.textPrimary
                font: root.timeFont
                width: root.fixedCellW
                height: root.lineHeight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            DigitSlot { id: slotM1 }
            DigitSlot { id: slotM2 }
        }
    }

    // Fixed-width digit slot. Holds a two-row column of Text (previous
    // digit above, current digit below). On each digit change the column
    // slides up by one cell so the previous digit scrolls off the top
    // while the new digit rises into view from below. clip: true
    // masks the column to the slot's bounds, hiding whichever row is
    // currently off-screen.
    component DigitSlot: Item {
        id: slot
        property string prev: ""
        property string curr: ""
        property real offsetY: 0

        width: root.fixedCellW
        height: root.lineHeight
        clip: true

        function setDigit(d) {
            if (d === curr)
                return
            prev = curr
            curr = d
            // Snap offsetY back to 0 first. The previous animation left
            // prev text identical to curr text, so this snap is visually
            // a no-op; it just re-arms the slot for the new roll.
            offsetY = 0
            rollAnim.restart()
        }

        SequentialAnimation {
            id: rollAnim
            NumberAnimation {
                target: slot
                property: "offsetY"
                to: -root.lineHeight
                duration: root.rollDuration
                easing.type: Easing.BezierSpline
                easing.bezierCurve: root.fadeCurve
            }
            onFinished: {
                // After the roll, prev text equals curr text. Resetting
                // offsetY back to 0 is therefore invisible and leaves the
                // slot ready for the next change.
                slot.prev = slot.curr
                slot.offsetY = 0
            }
        }

        Column {
            x: 0
            y: slot.offsetY
            spacing: 0
            Text {
                text: slot.prev
                color: StyleTokens.textPrimary
                font: root.timeFont
                width: slot.width
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                text: slot.curr
                color: StyleTokens.textPrimary
                font: root.timeFont
                width: slot.width
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
