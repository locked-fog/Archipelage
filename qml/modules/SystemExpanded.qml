import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property real volume: 0
    property bool muted: false
    property real brightness: 0
    property string volumeError: ""
    property string brightnessError: ""
    property string tlpProfile: ""
    property string tlpError: ""

    Component.onCompleted: {
        SystemServices.requestVolume();
        SystemServices.requestBrightness();
        SystemServices.requestTlpState();
    }

    Connections {
        target: SystemServices

        function onVolumeSnapshotReady(value, isMuted, errorString) {
            root.volumeError = errorString;
            if (errorString === "") {
                root.volume = value;
                root.muted = isMuted;
            }
        }

        function onVolumeSetFinished(value, success, errorString) {
            root.volumeError = errorString;
            if (success && errorString === "")
                root.volume = value;
        }

        function onBrightnessSnapshotReady(value, errorString) {
            root.brightnessError = errorString;
            if (errorString === "")
                root.brightness = value;
        }

        function onBrightnessSetFinished(value, success, errorString) {
            root.brightnessError = errorString;
            if (success && errorString === "")
                root.brightness = value;
        }

        function onTlpStateReady(available, profile, output, errorString) {
            root.tlpProfile = available ? profile : "";
            root.tlpError = errorString;
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 20

        Text {
            id: title

            text: "System"
            color: StyleTokens.textPrimary
            font.pixelSize: ArchipelagoConfig.titleFontSize
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
        }

        Text {
            anchors.right: parent.right
            anchors.verticalCenter: title.verticalCenter
            text: SysBackend.batteryCapacity > 0 ? SysBackend.batteryCapacity + "% " + SysBackend.batteryStatus : "Battery unknown"
            color: SysBackend.batteryCapacity > 0 && SysBackend.batteryCapacity <= 20 ? StyleTokens.warning : StyleTokens.textSecondary
            font.pixelSize: 12
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: title.bottom
            anchors.topMargin: 18
            spacing: 12

            ControlSlider {
                width: parent.width
                height: 86
                title: "Volume"
                value: root.volume
                valueText: root.muted ? "Muted" : Math.round(root.volume * 100) + "%"

                onValueMoved: function(value) {
                    root.volume = value;
                }
                onCommitRequested: SystemServices.setVolume(root.volume)
            }

            ControlSlider {
                width: parent.width
                height: 86
                title: "Brightness"
                value: root.brightness
                valueText: Math.round(root.brightness * 100) + "%"

                onValueMoved: function(value) {
                    root.brightness = value;
                }
                onCommitRequested: SystemServices.setBrightness(root.brightness)
            }

            Row {
                width: parent.width
                spacing: 12

                StatusPill {
                    width: (parent.width - 24) / 3
                    label: "niri"
                    value: Compositor.active ? "active" : "offline"
                    accent: Compositor.active ? StyleTokens.success : StyleTokens.warning
                }

                StatusPill {
                    width: (parent.width - 24) / 3
                    label: "power"
                    value: root.tlpProfile !== "" ? root.tlpProfile : "default"
                    accent: StyleTokens.accent
                }

                StatusPill {
                    width: (parent.width - 24) / 3
                    label: "record"
                    value: SystemServices.screenRecordingActive ? "on" : "off"
                    accent: SystemServices.screenRecordingActive ? StyleTokens.danger : StyleTokens.textSecondary
                }
            }

            Text {
                width: parent.width
                text: [ArchipelagoConfig.configError, Compositor.lastError, root.volumeError, root.brightnessError, root.tlpError]
                    .filter((entry) => entry !== "")
                    .join("  ")
                color: StyleTokens.warning
                font.pixelSize: 12
                font.family: ArchipelagoConfig.textFontFamily
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
                visible: text !== ""
            }
        }
    }
}
