import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property int compactLevel: 0
    property real volume: 0
    property bool muted: false

    function batteryIcon() {
        if (SysBackend.batteryStatus === "Charging")
            return "AC";
        if (SysBackend.batteryCapacity <= 20)
            return "BAT";
        return "BAT";
    }

    Component.onCompleted: {
        SystemServices.requestVolume();
    }

    Connections {
        target: SystemServices

        function onVolumeSnapshotReady(value, isMuted, errorString) {
            if (errorString !== "")
                return;
            root.volume = value;
            root.muted = isMuted;
        }

        function onVolumeSetFinished(value, success, errorString) {
            if (!success || errorString !== "")
                return;
            root.volume = value;
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: root.compactLevel >= 2 ? 7 : 10

        Text {
            text: root.muted ? "VOL 0" : "VOL " + Math.round(root.volume * 100)
            color: StyleTokens.textPrimary
            font.pixelSize: 12
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
            visible: root.compactLevel < 2
        }

        Text {
            text: root.muted ? "M" : Math.round(root.volume * 100)
            color: root.muted ? StyleTokens.warning : StyleTokens.textPrimary
            font.pixelSize: 12
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
            visible: root.compactLevel >= 2
        }

        Rectangle {
            width: 1
            height: 14
            color: StyleTokens.track
            visible: root.compactLevel < 2
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: batteryIcon() + " " + SysBackend.batteryCapacity + "%"
            color: SysBackend.batteryCapacity > 0 && SysBackend.batteryCapacity <= 20
                ? StyleTokens.warning
                : StyleTokens.textPrimary
            font.pixelSize: 12
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
