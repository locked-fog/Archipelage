import ArchipelagoCore
import ArchipelagoPlugins.Media 1.0
import QtQuick

Item {
    id: root

    property int compactLevel: 0
    property string moduleId: "media"
    property var shellWindow: null
    property var currentTrackState: ({
        "key": "__empty__",
        "title": "",
        "artUrl": "",
        "available": false
    })
    property var previousTrackState: currentTrackState
    property bool transitionActive: false
    property real transitionProgress: 1
    property int transitionDirection: 1
    property int pendingTransitionDirection: 0
    property string pressedButton: ""
    property bool buttonPressArmed: false
    property bool buttonPressInside: false
    property bool suppressNextClick: false
    readonly property int layoutPaddingLeft: compactLevel >= 2 ? 12 : 14
    readonly property int layoutPaddingRight: compactLevel >= 2 ? 10 : 12
    readonly property int layoutSpacing: compactLevel >= 2 ? 8 : 10
    readonly property string clientId: "compact:" + moduleId
    readonly property int coverSize: compactLevel >= 2 ? 24 : 28
    readonly property int barWidth: compactLevel >= 2 ? 18 : 22
    readonly property int controlSize: compactLevel >= 2 ? 20 : 22
    readonly property int controlSpacing: compactLevel >= 2 ? 4 : 5
    readonly property int controlsWidth: controlSize * 3 + controlSpacing * 2
    readonly property real slideDistance: Math.max(compactLevel >= 2 ? 124 : 152,
                                                   trackViewport.width + root.layoutSpacing * 2)
    readonly property int compactLayoutPriority: 70
    readonly property bool compactVisibleRequested: MediaService.available
    readonly property bool contentVisible: currentTrackState.available || previousTrackState.available || transitionActive
    readonly property int minimumCompactWidth: compactLevel >= 2 ? 188 : 216
    readonly property int maximumCompactWidth: compactLevel >= 2 ? 320 : 360
    readonly property int preferredCompactWidth: {
        if (!compactVisibleRequested)
            return minimumCompactWidth;
        const measuredTitleWidth = Math.ceil(titleMeasure.contentWidth);
        const preferredTitleWidth = Math.max(compactLevel >= 2 ? 78 : 104,
                                             Math.min(compactLevel >= 2 ? 148 : 210,
                                                      measuredTitleWidth + 22));
        const chromeWidth = layoutPaddingLeft + layoutPaddingRight
            + barWidth
            + coverSize
            + controlsWidth
            + layoutSpacing * 4;
        return Math.max(minimumCompactWidth,
                        Math.min(maximumCompactWidth, chromeWidth + preferredTitleWidth));
    }
    property var handlers: ({
        "primaryClicked": function() {
            if (root.suppressNextClick) {
                suppressClickReset.stop();
                root.suppressNextClick = false;
                return true;
            }
            return false;
        },
        "secondaryClicked": function() {
            return true;
        },
        "wheelMoved": function(delta) {
            if (!MediaService.available)
                return ;

            MediaService.setVolume(MediaService.volume + (delta > 0 ? 0.04 : -0.04));
        },
        "pointerPressed": function(x, y, button) {
            suppressClickReset.stop();
            root.suppressNextClick = false;
            root.pressedButton = "";
            root.buttonPressArmed = false;
            root.buttonPressInside = false;

            if (button !== Qt.LeftButton)
                return;

            const hit = root.buttonAt(x, y);
            if (hit === "")
                return;

            root.pressedButton = hit;
            root.buttonPressArmed = true;
            root.buttonPressInside = true;
        },
        "pointerMoved": function(x, y, buttons) {
            if (!root.buttonPressArmed || !(buttons & Qt.LeftButton))
                return;

            root.buttonPressInside = root.buttonAt(x, y) === root.pressedButton;
        },
        "pointerReleased": function(x, y, button) {
            if (button !== Qt.LeftButton || !root.buttonPressArmed) {
                root.pressedButton = "";
                root.buttonPressArmed = false;
                root.buttonPressInside = false;
                return;
            }

            const hit = root.buttonAt(x, y);
            const activate = root.buttonPressInside && hit === root.pressedButton;
            root.suppressNextClick = true;
            suppressClickReset.restart();

            if (activate)
                root.triggerButton(root.pressedButton);

            root.pressedButton = "";
            root.buttonPressArmed = false;
            root.buttonPressInside = false;
        },
        "pointerCanceled": function() {
            root.pressedButton = "";
            root.buttonPressArmed = false;
            root.buttonPressInside = false;
            root.suppressNextClick = false;
            suppressClickReset.stop();
        }
    })

    function compactTrackState() {
        const title = MediaService.title || MediaService.identity || "Media";
        const artist = MediaService.artist || "";
        const displayTitle = MediaService.available && artist !== "" && artist !== title ? title + " - " + artist : title;
        return {
            "key": MediaService.available
                ? [MediaService.activeService, title, artist, MediaService.album, MediaService.artUrl, String(MediaService.duration)].join("|")
                : "__empty__",
            "title": MediaService.available ? displayTitle : "",
            "artUrl": MediaService.artUrl,
            "available": MediaService.available
        };
    }

    function syncTrackState() {
        const nextState = compactTrackState();
        if (currentTrackState.key === undefined) {
            currentTrackState = nextState;
            previousTrackState = nextState;
            return;
        }
        if (!nextState.available) {
            transitionActive = false;
            previousTrackState = currentTrackState;
            pendingTransitionDirection = 0;
            transitionProgress = 1;
            return;
        }
        if (!currentTrackState.available) {
            currentTrackState = nextState;
            previousTrackState = nextState;
            return;
        }
        if (currentTrackState.key === nextState.key) {
            currentTrackState = nextState;
            return;
        }
        previousTrackState = currentTrackState;
        currentTrackState = nextState;
        transitionDirection = pendingTransitionDirection !== 0 ? pendingTransitionDirection : 1;
        pendingTransitionDirection = 0;
        transitionActive = true;
        transitionProgress = 0;
        compactTransition.restart();
    }

    function buttonEnabled(buttonId) {
        if (buttonId === "previous")
            return MediaService.canGoPrevious;
        if (buttonId === "play")
            return MediaService.canControl && (MediaService.canPlay || MediaService.canPause);
        if (buttonId === "next")
            return MediaService.canGoNext;
        return false;
    }

    function buttonAt(x, y) {
        if (!contentVisible)
            return "";

        const top = Math.round((height - controlSize) / 2);
        const left = width - layoutPaddingRight - controlsWidth;
        if (y < top || y > top + controlSize)
            return "";

        const slot = controlSize + controlSpacing;
        if (x >= left && x <= left + controlSize)
            return buttonEnabled("previous") ? "previous" : "";
        if (x >= left + slot && x <= left + slot + controlSize)
            return buttonEnabled("play") ? "play" : "";
        if (x >= left + slot * 2 && x <= left + slot * 2 + controlSize)
            return buttonEnabled("next") ? "next" : "";
        return "";
    }

    function triggerButton(buttonId) {
        if (buttonId === "previous") {
            pendingTransitionDirection = -1;
            MediaService.previous();
            return;
        }
        if (buttonId === "play") {
            MediaService.playPause();
            return;
        }
        if (buttonId === "next") {
            pendingTransitionDirection = 1;
            MediaService.next();
        }
    }

    Component.onCompleted: {
        MediaService.registerClient(clientId);
        CavaService.registerClient(clientId);
        currentTrackState = compactTrackState();
        previousTrackState = currentTrackState;
    }
    Component.onDestruction: {
        MediaService.releaseClient(clientId);
        CavaService.releaseClient(clientId);
    }

    Connections {
        target: MediaService

        function onStateChanged() {
            root.syncTrackState();
        }
    }

    NumberAnimation {
        id: compactTransition

        target: root
        property: "transitionProgress"
        from: 0
        to: 1
        duration: 320
        easing.type: Easing.OutCubic
        onStopped: {
            root.transitionActive = false;
            root.previousTrackState = root.currentTrackState;
            root.transitionProgress = 1;
        }
    }

    Timer {
        id: suppressClickReset

        interval: 180
        repeat: false
        onTriggered: root.suppressNextClick = false
    }

    Text {
        id: titleMeasure

        visible: false
        text: currentTrackState.title
        font.pixelSize: compactLevel >= 2 ? 11 : 12
        font.family: ArchipelagoConfig.textFontFamily
        font.weight: Font.DemiBold
    }

    Item {
        id: trackViewport

        anchors.left: parent.left
        anchors.right: controlsRow.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.layoutPaddingLeft
        anchors.rightMargin: root.layoutSpacing
        clip: true
        visible: root.contentVisible

        CompactTrackLayer {
            objectName: "outgoingTrackLayer"
            y: 0
            width: parent.width
            height: parent.height
            trackData: root.previousTrackState
            interactionEnabled: false
            visible: root.transitionActive
            x: -root.transitionDirection * root.transitionProgress * root.slideDistance
            opacity: 1
            z: 1
        }

        CompactTrackLayer {
            objectName: "incomingTrackLayer"
            y: 0
            width: parent.width
            height: parent.height
            trackData: root.currentTrackState
            interactionEnabled: !root.transitionActive
            x: root.transitionActive ? root.transitionDirection * (1 - root.transitionProgress) * root.slideDistance : 0
            opacity: 1
            z: 2
        }
    }

    Row {
        id: controlsRow

        anchors.right: parent.right
        anchors.rightMargin: root.layoutPaddingRight
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.controlSpacing
        visible: root.contentVisible

        CompactActionButton {
            icon: "previous"
            size: root.controlSize
            enabled: root.buttonEnabled("previous")
            pressed: root.buttonPressArmed && root.pressedButton === "previous" && root.buttonPressInside
        }

        CompactActionButton {
            icon: MediaService.playing ? "pause" : "play"
            size: root.controlSize
            emphasized: true
            enabled: root.buttonEnabled("play")
            pressed: root.buttonPressArmed && root.pressedButton === "play" && root.buttonPressInside
        }

        CompactActionButton {
            icon: "next"
            size: root.controlSize
            enabled: root.buttonEnabled("next")
            pressed: root.buttonPressArmed && root.pressedButton === "next" && root.buttonPressInside
        }
    }

    component CompactTrackLayer: Item {
        id: trackLayer

        property var trackData: ({
            "key": "__empty__",
            "title": "",
            "artUrl": "",
            "available": false
        })
        property bool interactionEnabled: false

        Row {
            anchors.fill: parent
            spacing: root.layoutSpacing

            AudioBars {
                width: root.barWidth
                height: 18
                anchors.verticalCenter: parent.verticalCenter
                active: MediaService.playing
                levels: CavaService.levels
            }

            Rectangle {
                width: root.coverSize
                height: root.coverSize
                radius: 5
                color: StyleTokens.module
                clip: true
                anchors.verticalCenter: parent.verticalCenter

                Image {
                    anchors.fill: parent
                    source: trackLayer.trackData.artUrl || ""
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }

                MediaIcon {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    icon: "note"
                    color: StyleTokens.textSecondary
                    visible: (trackLayer.trackData.artUrl || "") === ""
                }
            }

            Item {
                id: titleBox

                property bool hovered: trackLayer.interactionEnabled && titleHover.hovered
                property bool shouldScroll: titleText.implicitWidth > width + 4
                property real marqueeOffset: 0
                readonly property real scrollDistance: Math.max(0, titleText.implicitWidth - width + 18)

                width: Math.max(56, parent.width - root.barWidth - root.coverSize - parent.spacing * 2)
                height: parent.height
                clip: true

                onHoveredChanged: {
                    if (hovered && shouldScroll) {
                        marquee.restart();
                    } else {
                        marquee.stop();
                        marqueeOffset = 0;
                    }
                }
                onShouldScrollChanged: {
                    if (!shouldScroll) {
                        marquee.stop();
                        marqueeOffset = 0;
                    } else if (hovered) {
                        marquee.restart();
                    }
                }

                Text {
                    id: titleText

                    y: Math.round((parent.height - height) / 2)
                    x: -titleBox.marqueeOffset
                    width: titleBox.shouldScroll ? implicitWidth : titleBox.width
                    text: trackLayer.trackData.title || ""
                    color: StyleTokens.textPrimary
                    font.pixelSize: compactLevel >= 2 ? 11 : 12
                    font.family: ArchipelagoConfig.textFontFamily
                    font.weight: Font.DemiBold
                    elide: titleBox.hovered ? Text.ElideNone : Text.ElideRight
                    maximumLineCount: 1
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 22
                    visible: titleBox.shouldScroll && !titleBox.hovered

                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            position: 0
                            color: "#00000000"
                        }

                        GradientStop {
                            position: 1
                            color: StyleTokens.panel
                        }
                    }
                }

                SequentialAnimation {
                    id: marquee

                    loops: Animation.Infinite
                    running: false

                    PauseAnimation {
                        duration: 260
                    }

                    NumberAnimation {
                        target: titleBox
                        property: "marqueeOffset"
                        from: 0
                        to: titleBox.scrollDistance
                        duration: Math.max(1800, titleBox.scrollDistance * 24)
                        easing.type: Easing.Linear
                    }

                    PauseAnimation {
                        duration: 180
                    }

                    ScriptAction {
                        script: titleBox.marqueeOffset = 0
                    }
                }

                HoverHandler {
                    id: titleHover

                    acceptedDevices: PointerDevice.Mouse
                }
            }
        }
    }

    component CompactActionButton: Rectangle {
        id: button

        property string icon: "play"
        property int size: 22
        property bool emphasized: false
        property bool pressed: false

        width: size
        height: size
        radius: size / 2
        color: !enabled ? StyleTokens.transparent
            : (pressed ? StyleTokens.accent : (emphasized ? Qt.rgba(1, 1, 1, 0.08) : StyleTokens.transparent))
        border.width: enabled ? 1 : 0
        border.color: pressed ? StyleTokens.accent : Qt.rgba(1, 1, 1, emphasized ? 0.18 : 0.1)
        opacity: enabled ? 1 : 0.36

        MediaIcon {
            anchors.centerIn: parent
            width: Math.round(button.size * 0.58)
            height: Math.round(button.size * 0.58)
            icon: button.icon
            color: button.pressed ? StyleTokens.textPrimary : StyleTokens.textSecondary
        }
    }

    component AudioBars: Item {
        id: bars

        property bool active: false
        property var levels: []
        readonly property int barCount: levels && levels.length > 0 ? levels.length : 6

        Row {
            anchors.centerIn: parent
            spacing: 2

            Repeater {
                model: bars.barCount

                Rectangle {
                    required property int index

                    readonly property real rawLevel: index < bars.levels.length ? Number(bars.levels[index] || 0) : 0
                    readonly property real clampedLevel: Math.max(0, Math.min(1, rawLevel))

                    width: compactLevel >= 2 ? 2 : 3
                    height: Math.max(4, Math.round(4 + clampedLevel * (bars.height - 5)))
                    radius: width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: bars.active ? StyleTokens.accent : StyleTokens.textSecondary
                    opacity: bars.active ? 1 : 0.7

                    Behavior on height {
                        NumberAnimation {
                            duration: 95
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }
    }

    component MediaIcon: Canvas {
        id: iconCanvas

        property string icon: "play"
        property color color: StyleTokens.textPrimary

        function triangle(ctx, x1, y1, x2, y2, x3, y3) {
            ctx.beginPath();
            ctx.moveTo(x1, y1);
            ctx.lineTo(x2, y2);
            ctx.lineTo(x3, y3);
            ctx.closePath();
            ctx.fill();
        }

        antialiasing: true
        onIconChanged: requestPaint()
        onColorChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            ctx.fillStyle = color;
            ctx.strokeStyle = color;
            ctx.lineWidth = Math.max(1.4, Math.min(width, height) * 0.12);
            ctx.lineCap = "round";
            ctx.lineJoin = "round";
            const w = width;
            const h = height;
            if (icon === "pause") {
                ctx.fillRect(w * 0.2, h * 0.12, w * 0.22, h * 0.76);
                ctx.fillRect(w * 0.58, h * 0.12, w * 0.22, h * 0.76);
            } else if (icon === "previous") {
                triangle(ctx, w * 0.88, h * 0.14, w * 0.52, h * 0.5, w * 0.88, h * 0.86);
                triangle(ctx, w * 0.58, h * 0.14, w * 0.22, h * 0.5, w * 0.58, h * 0.86);
                ctx.fillRect(w * 0.1, h * 0.14, w * 0.08, h * 0.72);
            } else if (icon === "next") {
                triangle(ctx, w * 0.12, h * 0.14, w * 0.48, h * 0.5, w * 0.12, h * 0.86);
                triangle(ctx, w * 0.42, h * 0.14, w * 0.78, h * 0.5, w * 0.42, h * 0.86);
                ctx.fillRect(w * 0.82, h * 0.14, w * 0.08, h * 0.72);
            } else if (icon === "note") {
                ctx.beginPath();
                ctx.moveTo(w * 0.62, h * 0.16);
                ctx.lineTo(w * 0.62, h * 0.68);
                ctx.stroke();
                ctx.beginPath();
                ctx.arc(w * 0.42, h * 0.72, w * 0.16, 0, Math.PI * 2);
                ctx.fill();
                ctx.beginPath();
                ctx.moveTo(w * 0.62, h * 0.18);
                ctx.lineTo(w * 0.84, h * 0.26);
                ctx.stroke();
            } else {
                triangle(ctx, w * 0.24, h * 0.1, w * 0.82, h * 0.5, w * 0.24, h * 0.9);
            }
        }
    }
}
