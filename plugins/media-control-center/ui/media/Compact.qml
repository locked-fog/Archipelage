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
    property real swipeOffset: 0
    property bool suppressNextClick: false
    readonly property int layoutPaddingLeft: compactLevel >= 2 ? 12 : 14
    readonly property int layoutPaddingRight: compactLevel >= 2 ? 10 : 12
    readonly property int layoutSpacing: compactLevel >= 2 ? 8 : 10
    readonly property string clientId: "compact:" + moduleId
    readonly property int coverSize: compactLevel >= 2 ? 24 : 28
    readonly property int barWidth: compactLevel >= 2 ? 24 : 28
    readonly property int compactLayoutPriority: 70
    readonly property bool compactVisibleRequested: MediaService.available
    readonly property int minimumCompactWidth: compactLevel >= 2 ? 182 : 212
    readonly property int maximumCompactWidth: compactLevel >= 2 ? 318 : 356
    readonly property int preferredCompactWidth: {
        if (!compactVisibleRequested)
            return minimumCompactWidth;
        const measuredTitleWidth = Math.ceil(titleMeasure.contentWidth);
        const preferredTitleWidth = Math.max(compactLevel >= 2 ? 82 : 104,
                                             Math.min(compactLevel >= 2 ? 154 : 208,
                                                      measuredTitleWidth + 22));
        const chromeWidth = layoutPaddingLeft + layoutPaddingRight + barWidth + coverSize + layoutSpacing * 3;
        return Math.max(minimumCompactWidth,
                        Math.min(maximumCompactWidth, chromeWidth + preferredTitleWidth));
    }
    property var handlers: ({
        "primaryClicked": function() {
            return true;
        },
        "secondaryClicked": function() {
            return true;
        },
        "wheelMoved": function() {
            return ;
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

    function pointerInside(item) {
        if (!gestureArea.containsMouse || !item)
            return false;
        const point = item.mapFromItem(gestureArea, gestureArea.mouseX, gestureArea.mouseY);
        return point.x >= 0 && point.x <= item.width && point.y >= 0 && point.y <= item.height;
    }

    function clampSwipeOffset(rawValue) {
        const limit = compactLevel >= 2 ? 54 : 68;
        return Math.max(-limit, Math.min(limit, rawValue));
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
        duration: 280
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

    Behavior on swipeOffset {
        enabled: !gestureArea.pressed && !transitionActive

        NumberAnimation {
            duration: 170
            easing.type: Easing.OutCubic
        }
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
        id: viewport

        anchors.fill: parent
        clip: true
        visible: MediaService.available

        CompactTrackLayer {
            anchors.fill: parent
            trackData: root.previousTrackState
            interactionEnabled: false
            x: root.transitionActive ? root.transitionDirection * root.transitionProgress * (compactLevel >= 2 ? 48 : 64) : 0
            opacity: root.transitionActive ? (1 - root.transitionProgress) : 0
        }

        CompactTrackLayer {
            anchors.fill: parent
            trackData: root.currentTrackState
            interactionEnabled: !root.transitionActive
            x: root.transitionActive
                ? root.transitionDirection * (root.transitionProgress - 1) * (compactLevel >= 2 ? 48 : 64)
                : root.swipeOffset
            opacity: root.transitionActive ? Math.min(1, 0.2 + root.transitionProgress * 0.8) : 1
        }
    }

    MouseArea {
        id: gestureArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        enabled: MediaService.available
        hoverEnabled: true
        preventStealing: true

        property real pressX: 0
        property real pressY: 0

        onPressed: function(mouse) {
            suppressClickReset.stop();
            pressX = mouse.x;
            pressY = mouse.y;
            root.suppressNextClick = false;
        }

        onPositionChanged: function(mouse) {
            if (!(mouse.buttons & Qt.LeftButton))
                return;

            const deltaX = mouse.x - pressX;
            const deltaY = mouse.y - pressY;
            if (Math.abs(deltaY) > 24)
                return;
            if (Math.abs(deltaX) <= Math.abs(deltaY) + 4 && Math.abs(root.swipeOffset) < 0.5)
                return;

            root.swipeOffset = root.clampSwipeOffset(deltaX);
        }

        onReleased: function(mouse) {
            if (mouse.button !== Qt.LeftButton) {
                root.swipeOffset = 0;
                return;
            }

            const deltaX = mouse.x - pressX;
            const deltaY = mouse.y - pressY;
            const horizontalDrag = Math.abs(deltaX) > 10 && Math.abs(deltaX) > Math.abs(deltaY) + 4;
            const swipeThreshold = compactLevel >= 2 ? 38 : 48;
            root.suppressNextClick = horizontalDrag;
            if (root.suppressNextClick)
                suppressClickReset.restart();

            if (horizontalDrag && Math.abs(deltaX) >= swipeThreshold) {
                if (deltaX < 0 && MediaService.canGoPrevious) {
                    root.pendingTransitionDirection = -1;
                    MediaService.previous();
                } else if (deltaX > 0 && MediaService.canGoNext) {
                    root.pendingTransitionDirection = 1;
                    MediaService.next();
                }
            }

            root.swipeOffset = 0;
        }

        onClicked: function(mouse) {
            if (root.suppressNextClick) {
                suppressClickReset.stop();
                root.suppressNextClick = false;
                return;
            }

            if (mouse.button === Qt.RightButton) {
                MediaService.playPause();
                return;
            }

            if (shellWindow)
                shellWindow.toggleModule(root.moduleId);
        }

        onCanceled: {
            suppressClickReset.stop();
            root.suppressNextClick = false;
            root.swipeOffset = 0;
        }

        onWheel: function(wheel) {
            if (!MediaService.available)
                return;

            MediaService.setVolume(MediaService.volume + (wheel.angleDelta.y > 0 ? 0.04 : -0.04));
            wheel.accepted = true;
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
            anchors.leftMargin: root.layoutPaddingLeft
            anchors.rightMargin: root.layoutPaddingRight
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

                property bool hovered: trackLayer.interactionEnabled && root.pointerInside(titleBox)
                property bool shouldScroll: titleText.implicitWidth > width + 4
                property real marqueeOffset: 0
                readonly property real scrollDistance: Math.max(0, titleText.implicitWidth - width + 18)

                width: Math.max(72, parent.width - root.barWidth - root.coverSize - parent.spacing * 2)
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
            }
        }
    }

    component AudioBars: Item {
        id: bars

        property bool active: false
        property var levels: []
        readonly property int barCount: levels && levels.length > 0 ? levels.length : 8

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
