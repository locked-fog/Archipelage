import ArchipelagoCore
import ArchipelagoPlugins.Media 1.0
import QtQuick

Item {
    id: root

    property int compactLevel: 0
    property string moduleId: "media"
    readonly property int layoutPaddingLeft: 10
    readonly property int layoutPaddingRight: 8
    readonly property int layoutSpacing: 8
    property var handlers: ({
        "primaryClicked": function() {
            return false;
        },
        "secondaryClicked": function() {
            MediaService.playPause();
            return true;
        },
        "wheelMoved": function(delta) {
            if (!MediaService.available)
                return ;

            MediaService.setVolume(MediaService.volume + (delta > 0 ? 0.04 : -0.04));
        }
    })
    readonly property string clientId: "compact:" + moduleId
    readonly property string titleText: MediaService.available ? (MediaService.title || MediaService.identity || "Media") : ""
    readonly property int coverSize: compactLevel >= 2 ? 24 : 28
    readonly property int controlSize: compactLevel >= 2 ? 22 : 24
    readonly property int compactLayoutPriority: 70
    readonly property bool compactVisibleRequested: MediaService.available
    readonly property int minimumCompactWidth: compactLevel >= 2 ? 164 : 188
    readonly property int maximumCompactWidth: compactLevel >= 2 ? 276 : 320
    readonly property int preferredCompactWidth: {
        if (!compactVisibleRequested)
            return minimumCompactWidth;
        const measuredTitleWidth = Math.ceil(titleMeasure.contentWidth);
        const preferredTitleWidth = Math.max(compactLevel >= 2 ? 46 : 54,
                                             Math.min(compactLevel >= 2 ? 96 : 132,
                                                      measuredTitleWidth + 12));
        const chromeWidth = layoutPaddingLeft + layoutPaddingRight
            + (compactLevel >= 2 ? 16 : 20)
            + coverSize
            + controlSize * 3
            + layoutSpacing * 5;
        return Math.max(minimumCompactWidth,
                        Math.min(maximumCompactWidth, chromeWidth + preferredTitleWidth));
    }

    Component.onCompleted: {
        MediaService.registerClient(clientId);
    }
    Component.onDestruction: {
        MediaService.releaseClient(clientId);
    }

    Text {
        id: titleMeasure

        visible: false
        text: root.titleText
        font.pixelSize: compactLevel >= 2 ? 11 : 12
        font.family: ArchipelagoConfig.textFontFamily
        font.weight: Font.DemiBold
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: root.layoutPaddingLeft
        anchors.rightMargin: root.layoutPaddingRight
        spacing: root.layoutSpacing
        visible: MediaService.available

        AudioBars {
            width: compactLevel >= 2 ? 16 : 20
            height: 18
            anchors.verticalCenter: parent.verticalCenter
            active: MediaService.playing
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
                source: MediaService.artUrl
                fillMode: Image.PreserveAspectCrop
                visible: status === Image.Ready
            }

            MediaIcon {
                anchors.centerIn: parent
                width: 14
                height: 14
                icon: "note"
                color: StyleTokens.textSecondary
                visible: MediaService.artUrl === ""
            }

        }

        Item {
            id: titleBox

            property bool hovered: titleHover.containsMouse
            property bool shouldScroll: titleText.contentWidth > width + 4
            readonly property real scrollDistance: Math.max(0, titleText.contentWidth - width + 16)

            width: Math.max(40, parent.width - 20 - root.coverSize - root.controlSize * 3 - parent.spacing * 5)
            height: parent.height
            clip: true

            Text {
                id: titleText

                y: Math.round((parent.height - height) / 2)
                x: titleBox.hovered && titleBox.shouldScroll ? -titleBox.scrollDistance : 0
                width: titleBox.shouldScroll ? implicitWidth + 16 : titleBox.width
                text: root.titleText
                color: StyleTokens.textPrimary
                font.pixelSize: compactLevel >= 2 ? 11 : 12
                font.family: ArchipelagoConfig.textFontFamily
                font.weight: Font.DemiBold
                elide: titleBox.hovered ? Text.ElideNone : Text.ElideRight
                maximumLineCount: 1

                Behavior on x {
                    NumberAnimation {
                        duration: Math.max(900, titleBox.scrollDistance * 28)
                        easing.type: Easing.InOutQuad
                    }

                }

            }

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 18
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

            MouseArea {
                id: titleHover

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }

        }

        CompactButton {
            icon: "previous"
            enabled: MediaService.canGoPrevious
            size: root.controlSize
            onClicked: MediaService.previous()
        }

        CompactButton {
            icon: MediaService.playing ? "pause" : "play"
            enabled: MediaService.canControl && (MediaService.canPlay || MediaService.canPause)
            size: root.controlSize
            emphasized: true
            onClicked: MediaService.playPause()
        }

        CompactButton {
            icon: "next"
            enabled: MediaService.canGoNext
            size: root.controlSize
            onClicked: MediaService.next()
        }

    }

    component AudioBars: Item {
        id: bars

        property bool active: false

        Row {
            anchors.centerIn: parent
            spacing: 2

            Repeater {
                model: 4

                Rectangle {
                    required property int index

                    width: 3
                    height: bars.active ? 8 + ((index * 5 + pulse.tick) % 10) : 5
                    radius: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: bars.active ? StyleTokens.accent : StyleTokens.textSecondary

                    Behavior on height {
                        NumberAnimation {
                            duration: 140
                        }

                    }

                }

            }

        }

        Timer {
            id: pulse

            property int tick: 0

            interval: 180
            running: bars.active
            repeat: true
            onTriggered: tick += 1
        }

    }

    component CompactButton: Rectangle {
        id: button

        property string icon: "play"
        property bool emphasized: false
        property int size: 24

        signal clicked()

        width: size
        height: size
        radius: size / 2
        color: emphasized && enabled ? StyleTokens.accent : StyleTokens.transparent
        opacity: enabled ? 1 : 0.42
        anchors.verticalCenter: parent.verticalCenter

        MediaIcon {
            anchors.centerIn: parent
            width: Math.round(button.size * 0.58)
            height: Math.round(button.size * 0.58)
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
