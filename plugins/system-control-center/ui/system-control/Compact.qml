import QtQuick
import ArchipelagoCore
import ArchipelagoBackend
import ArchipelagoPlugins.SystemControl 1.0

Item {
    id: root

    property int compactLevel: 0
    property string moduleId: "system-control"
    property var shellWindow: null
    readonly property string clientId: "compact:" + moduleId
    readonly property int compactLayoutPriority: 65
    readonly property bool compactVisibleRequested: true
    readonly property int minimumCompactWidth: compactLevel >= 2 ? 116 : 126
    readonly property int maximumCompactWidth: compactLevel >= 2 ? 152 : 164
    readonly property int preferredCompactWidth: compactLevel >= 2 ? 126 : 138
    readonly property int glyphSize: compactLevel >= 2 ? 15 : 17
    readonly property int spacing: compactLevel >= 2 ? 10 : 12
    property int wifiSearchPhase: 0

    property var handlers: ({
        "primaryClicked": function() {
            return false
        },
        "secondaryClicked": function() {
            return true
        },
        "wheelMoved": function(delta) {
            AudioRouteService.setVolume(AudioRouteService.volume + (delta > 0 ? 0.04 : -0.04))
        }
    })

    Component.onCompleted: {
        ConnectivityService.registerClient(clientId)
        AudioRouteService.registerClient(clientId)
        DisplayService.registerClient(clientId)
    }

    Component.onDestruction: {
        ConnectivityService.releaseClient(clientId)
        AudioRouteService.releaseClient(clientId)
        DisplayService.releaseClient(clientId)
    }

    Timer {
        running: ConnectivityService.wifiEnabled
            && !ConnectivityService.wiredConnected
            && !ConnectivityService.wifiConnected
        repeat: true
        interval: 460
        onTriggered: {
            root.wifiSearchPhase = (root.wifiSearchPhase + 1) % 3
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: root.spacing

        StatusDot {
            warning: ConnectivityService.lastError !== ""

            NetworkGlyph {
                anchors.centerIn: parent
                width: root.glyphSize
                height: root.glyphSize
                wiredConnected: ConnectivityService.wiredConnected
                wifiEnabled: ConnectivityService.wifiEnabled
                wifiConnected: ConnectivityService.wifiConnected
                signalStrength: ConnectivityService.activeSignal
                searchPhase: root.wifiSearchPhase
                activeColor: StyleTokens.textPrimary
                inactiveColor: ConnectivityService.lastError !== ""
                    ? StyleTokens.danger
                    : StyleTokens.textSecondary
            }
        }

        StatusDot {
            active: !AudioRouteService.muted && AudioRouteService.volume > 0.01
            warning: AudioRouteService.lastError !== ""

            VolumeGlyph {
                anchors.centerIn: parent
                width: root.glyphSize
                height: root.glyphSize
                volume: AudioRouteService.volume
                muted: AudioRouteService.muted
                activeColor: StyleTokens.textPrimary
                inactiveColor: AudioRouteService.lastError !== ""
                    ? StyleTokens.danger
                    : StyleTokens.textSecondary
            }
        }

        StatusDot {
            active: DisplayService.brightness >= 0.3

            BrightnessGlyph {
                anchors.centerIn: parent
                width: root.glyphSize
                height: root.glyphSize
                brightness: DisplayService.brightness
                activeColor: StyleTokens.textPrimary
                inactiveColor: StyleTokens.textSecondary
            }
        }
    }

    component StatusDot: Item {
        id: dot

        property bool active: false
        property bool warning: false

        width: root.glyphSize + 6
        height: width

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: warning
                ? Qt.rgba(1.0, 0.43, 0.43, 0.18)
                : (dot.active ? Qt.rgba(0.39, 0.78, 1.0, 0.18) : Qt.rgba(1, 1, 1, 0.08))
            border.width: dot.active || warning ? 1 : 0
            border.color: warning ? StyleTokens.danger : Qt.rgba(0.39, 0.78, 1.0, 0.32)
        }
    }
}
