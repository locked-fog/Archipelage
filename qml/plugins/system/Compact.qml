import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property int compactLevel: 0
    property string moduleId: ""
    property var handlers: ({})
    property real volume: 0
    property bool muted: false

    function batteryIcon() {
        if (BatteryService.status === "Charging")
            return "AC";
        if (BatteryService.capacity <= 20)
            return "BAT";
        return "BAT";
    }

    Component.onCompleted: {
        VolumeService.requestVolume();
    }

    Connections {
        target: VolumeService

        function onVolumeChanged() {
            root.volume = VolumeService.volume;
            root.muted = VolumeService.muted;
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
            text: batteryIcon() + " " + BatteryService.capacity + "%"
            color: BatteryService.capacity > 0 && BatteryService.capacity <= 20
                ? StyleTokens.warning
                : StyleTokens.textPrimary
            font.pixelSize: 12
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
