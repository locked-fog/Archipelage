import ArchipelagoCore
import ArchipelagoPlugins.Media 1.0
import QtQuick

Item {
    id: root

    property string moduleId: "media"
    property var shellWindow: null
    readonly property string clientId: "expanded:" + moduleId
    readonly property real progress: MediaService.duration > 0 ? Math.max(0, Math.min(1, MediaService.position / MediaService.duration)) : 0

    function timeText(valueUs) {
        if (valueUs <= 0)
            return "0:00";

        const seconds = Math.floor(valueUs / 1e+06);
        const minutes = Math.floor(seconds / 60);
        const rest = seconds % 60;
        return minutes + ":" + (rest < 10 ? "0" : "") + rest;
    }

    function serviceLabel(entry) {
        if (!entry)
            return "";

        return entry.identity || entry.service || "";
    }

    function loopIcon() {
        return MediaService.loopStatus === "Track" ? "repeat-one" : "repeat";
    }

    function loopActive() {
        return MediaService.loopStatus === "Track" || MediaService.loopStatus === "Playlist";
    }

    Component.onCompleted: MediaService.registerClient(clientId)
    Component.onDestruction: MediaService.releaseClient(clientId)

    Item {
        anchors.fill: parent
        anchors.margins: 20

        Rectangle {
            id: art

            width: 108
            height: 108
            radius: 8
            color: StyleTokens.module
            clip: true

            Image {
                anchors.fill: parent
                source: MediaService.artUrl
                fillMode: Image.PreserveAspectCrop
                visible: status === Image.Ready
            }

            MediaIcon {
                anchors.centerIn: parent
                width: 34
                height: 34
                icon: "note"
                color: StyleTokens.textSecondary
                visible: MediaService.artUrl === ""
            }

        }

        Column {
            anchors.left: art.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 18
            spacing: 4

            Text {
                width: parent.width
                text: MediaService.available ? MediaService.title : "No active media player"
                color: StyleTokens.textPrimary
                font.pixelSize: 22
                font.family: ArchipelagoConfig.textFontFamily
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: MediaService.available ? (MediaService.artist || MediaService.identity) : "Start an MPRIS-compatible player"
                color: StyleTokens.textSecondary
                font.pixelSize: 13
                font.family: ArchipelagoConfig.textFontFamily
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                width: parent.width
                text: MediaService.album
                color: StyleTokens.textMuted
                font.pixelSize: 12
                font.family: ArchipelagoConfig.textFontFamily
                elide: Text.ElideRight
                maximumLineCount: 1
                visible: text !== ""
            }

        }

        Row {
            id: playerRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: art.bottom
            anchors.topMargin: 18
            spacing: 8

            Repeater {
                model: MediaService.players

                Rectangle {
                    required property var modelData

                    width: Math.min(150, Math.max(70, label.implicitWidth + 22))
                    height: 28
                    radius: 14
                    color: modelData.service === MediaService.activeService ? StyleTokens.accent : StyleTokens.module

                    Text {
                        id: label

                        anchors.centerIn: parent
                        width: parent.width - 16
                        text: root.serviceLabel(modelData)
                        color: StyleTokens.textPrimary
                        font.pixelSize: 11
                        font.family: ArchipelagoConfig.textFontFamily
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: MediaService.choosePlayer(modelData.service)
                    }

                }

            }

        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: playerRow.bottom
            anchors.topMargin: 18
            spacing: 8

            MediaSlider {
                width: parent.width
                value: root.progress
                enabled: MediaService.canSeek && MediaService.duration > 0
                accent: StyleTokens.accent
                onMoved: function(nextValue) {
                    MediaService.setPosition(Math.round(nextValue * MediaService.duration));
                }
            }

            Row {
                width: parent.width

                Text {
                    id: elapsed

                    text: root.timeText(MediaService.position)
                    color: StyleTokens.textSecondary
                    font.pixelSize: 11
                    font.family: ArchipelagoConfig.textFontFamily
                }

                Item {
                    width: Math.max(0, parent.width - elapsed.width - total.width)
                    height: 1
                }

                Text {
                    id: total

                    text: root.timeText(MediaService.duration)
                    color: StyleTokens.textSecondary
                    font.pixelSize: 11
                    font.family: ArchipelagoConfig.textFontFamily
                }

            }

        }

        Row {
            id: controlRow

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: volumeRow.top
            anchors.bottomMargin: 22
            spacing: 10

            ControlButton {
                icon: "shuffle"
                active: MediaService.shuffle
                enabled: MediaService.canControl
                onClicked: MediaService.toggleShuffle()
            }

            ControlButton {
                icon: "previous"
                enabled: MediaService.canGoPrevious
                onClicked: MediaService.previous()
            }

            ControlButton {
                icon: MediaService.playing ? "pause" : "play"
                emphasized: true
                enabled: MediaService.canControl && (MediaService.canPlay || MediaService.canPause)
                onClicked: MediaService.playPause()
            }

            ControlButton {
                icon: "stop"
                enabled: MediaService.canControl
                onClicked: MediaService.stop()
            }

            ControlButton {
                icon: "next"
                enabled: MediaService.canGoNext
                onClicked: MediaService.next()
            }

            ControlButton {
                icon: root.loopIcon()
                active: root.loopActive()
                enabled: MediaService.canControl
                onClicked: MediaService.toggleLoopStatus()
            }

        }

        Row {
            id: volumeRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: 10

            MediaIcon {
                width: 18
                height: 18
                anchors.verticalCenter: parent.verticalCenter
                icon: "volume"
                color: StyleTokens.textSecondary
            }

            MediaSlider {
                width: parent.width - 28
                value: Math.max(0, Math.min(1, MediaService.volume))
                enabled: MediaService.available
                accent: StyleTokens.success
                wheelStep: 0.04
                onMoved: function(nextValue) {
                    MediaService.setVolume(nextValue);
                }
            }

        }

    }

    component MediaSlider: Item {
        id: slider

        property real value: 0
        property color accent: StyleTokens.accent
        property real wheelStep: 0

        signal moved(real nextValue)

        function clamp(raw) {
            return Math.max(0, Math.min(1, raw));
        }

        function setFromX(localX) {
            moved(clamp(localX / Math.max(1, width)));
        }

        height: 24

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 8
            radius: 4
            color: StyleTokens.track

            Rectangle {
                width: parent.width * slider.clamp(slider.value)
                height: parent.height
                radius: parent.radius
                color: slider.enabled ? slider.accent : StyleTokens.textMuted
            }

        }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            x: slider.clamp(slider.value) * (slider.width - width)
            anchors.verticalCenter: parent.verticalCenter
            color: slider.enabled ? StyleTokens.textPrimary : StyleTokens.textMuted
            border.width: 2
            border.color: slider.enabled ? slider.accent : StyleTokens.track
        }

        MouseArea {
            anchors.fill: parent
            enabled: slider.enabled
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            onPressed: function(mouse) {
                slider.setFromX(mouse.x);
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    slider.setFromX(mouse.x);

            }
            onWheel: function(wheel) {
                if (slider.wheelStep <= 0)
                    return ;

                const direction = wheel.angleDelta.y > 0 ? 1 : -1;
                slider.moved(slider.clamp(slider.value + direction * slider.wheelStep));
                wheel.accepted = true;
            }
        }

    }

    component ControlButton: Rectangle {
        id: button

        property string icon: "play"
        property bool emphasized: false
        property bool active: false

        signal clicked()

        width: emphasized ? 48 : 36
        height: emphasized ? 48 : 36
        radius: width / 2
        color: !enabled ? StyleTokens.track : (emphasized || active ? StyleTokens.accent : StyleTokens.module)
        opacity: enabled ? 1 : 0.45

        MediaIcon {
            anchors.centerIn: parent
            width: button.emphasized ? 22 : 17
            height: button.emphasized ? 22 : 17
            icon: button.icon
            color: StyleTokens.textPrimary
        }

        MouseArea {
            anchors.fill: parent
            enabled: button.enabled
            onClicked: button.clicked()
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

        function line(ctx, x1, y1, x2, y2) {
            ctx.beginPath();
            ctx.moveTo(x1, y1);
            ctx.lineTo(x2, y2);
            ctx.stroke();
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
            } else if (icon === "stop") {
                ctx.fillRect(w * 0.22, h * 0.22, w * 0.56, h * 0.56);
            } else if (icon === "next") {
                triangle(ctx, w * 0.12, h * 0.14, w * 0.48, h * 0.5, w * 0.12, h * 0.86);
                triangle(ctx, w * 0.42, h * 0.14, w * 0.78, h * 0.5, w * 0.42, h * 0.86);
                ctx.fillRect(w * 0.82, h * 0.14, w * 0.08, h * 0.72);
            } else if (icon === "previous") {
                triangle(ctx, w * 0.88, h * 0.14, w * 0.52, h * 0.5, w * 0.88, h * 0.86);
                triangle(ctx, w * 0.58, h * 0.14, w * 0.22, h * 0.5, w * 0.58, h * 0.86);
                ctx.fillRect(w * 0.1, h * 0.14, w * 0.08, h * 0.72);
            } else if (icon === "volume") {
                ctx.beginPath();
                ctx.moveTo(w * 0.12, h * 0.42);
                ctx.lineTo(w * 0.32, h * 0.42);
                ctx.lineTo(w * 0.56, h * 0.2);
                ctx.lineTo(w * 0.56, h * 0.8);
                ctx.lineTo(w * 0.32, h * 0.58);
                ctx.lineTo(w * 0.12, h * 0.58);
                ctx.closePath();
                ctx.fill();
                ctx.beginPath();
                ctx.arc(w * 0.58, h * 0.5, w * 0.22, -0.75, 0.75);
                ctx.stroke();
            } else if (icon === "note") {
                line(ctx, w * 0.62, h * 0.16, w * 0.62, h * 0.68);
                ctx.beginPath();
                ctx.arc(w * 0.42, h * 0.72, w * 0.16, 0, Math.PI * 2);
                ctx.fill();
                line(ctx, w * 0.62, h * 0.18, w * 0.84, h * 0.26);
            } else if (icon === "shuffle") {
                line(ctx, w * 0.12, h * 0.3, w * 0.35, h * 0.3);
                line(ctx, w * 0.35, h * 0.3, w * 0.65, h * 0.7);
                line(ctx, w * 0.12, h * 0.7, w * 0.35, h * 0.7);
                line(ctx, w * 0.35, h * 0.7, w * 0.65, h * 0.3);
                line(ctx, w * 0.65, h * 0.3, w * 0.86, h * 0.3);
                line(ctx, w * 0.65, h * 0.7, w * 0.86, h * 0.7);
                triangle(ctx, w * 0.9, h * 0.3, w * 0.76, h * 0.22, w * 0.76, h * 0.38);
                triangle(ctx, w * 0.9, h * 0.7, w * 0.76, h * 0.62, w * 0.76, h * 0.78);
            } else if (icon === "repeat" || icon === "repeat-one") {
                line(ctx, w * 0.2, h * 0.34, w * 0.72, h * 0.34);
                line(ctx, w * 0.8, h * 0.42, w * 0.8, h * 0.66);
                line(ctx, w * 0.8, h * 0.66, w * 0.28, h * 0.66);
                line(ctx, w * 0.2, h * 0.58, w * 0.2, h * 0.34);
                triangle(ctx, w * 0.82, h * 0.34, w * 0.68, h * 0.24, w * 0.68, h * 0.44);
                triangle(ctx, w * 0.18, h * 0.66, w * 0.32, h * 0.56, w * 0.32, h * 0.76);
                if (icon === "repeat-one") {
                    ctx.font = Math.round(h * 0.38) + "px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "middle";
                    ctx.fillText("1", w * 0.5, h * 0.5);
                }
            } else {
                triangle(ctx, w * 0.24, h * 0.1, w * 0.82, h * 0.5, w * 0.24, h * 0.9);
            }
        }
    }

}
