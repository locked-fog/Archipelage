import QtQuick
import ArchipelagoCore
import ArchipelagoPlugins.SystemControl 1.0

Item {
    id: root

    property var payload: ({})
    property string instanceId: ""
    property var previewController: null
    property var shellWindow: null
    readonly property var discoverableDevices: BluetoothService.devices.filter(function(entry) {
        return !entry.paired
    })

    Component.onCompleted: BluetoothService.requestScan()

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Row {
            width: parent.width
            spacing: 8

            Text {
                width: parent.width - refreshButton.width - 8
                text: BluetoothService.discovering ? "Scanning for new devices" : "New Bluetooth devices"
                color: StyleTokens.textPrimary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            PreviewButton {
                id: refreshButton
                label: "Scan"
                onClicked: BluetoothService.requestScan()
            }
        }

        Column {
            width: parent.width
            spacing: 8

            Repeater {
                model: root.discoverableDevices

                Rectangle {
                    required property var modelData

                    width: parent.width
                    height: 52
                    radius: 16
                    color: StyleTokens.module

                    Row {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        SystemGlyph {
                            width: 18
                            height: 18
                            icon: "bluetooth"
                            color: StyleTokens.textPrimary
                        }

                        Column {
                            width: parent.width - 96
                            spacing: 2

                            Text {
                                width: parent.width
                                text: modelData.name
                                color: StyleTokens.textPrimary
                                font.family: ArchipelagoConfig.textFontFamily
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                text: "Discovered"
                                color: StyleTokens.textSecondary
                                font.family: ArchipelagoConfig.textFontFamily
                                font.pixelSize: 11
                            }
                        }

                        PreviewButton {
                            anchors.verticalCenter: parent.verticalCenter
                            label: "Connect"
                            onClicked: BluetoothService.connectDevice(modelData.address)
                        }
                    }
                }
            }

            Text {
                visible: root.discoverableDevices.length === 0
                text: BluetoothService.discovering
                    ? "Searching…"
                    : "No unpaired devices found yet."
                color: StyleTokens.textSecondary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 12
            }
        }
    }

    component PreviewButton: Rectangle {
        id: button

        property string label: ""
        signal clicked()

        implicitWidth: Math.ceil(labelNode.implicitWidth + 20)
        height: 28
        radius: 14
        color: StyleTokens.track

        Text {
            id: labelNode
            anchors.centerIn: parent
            text: button.label
            color: StyleTokens.textPrimary
            font.family: ArchipelagoConfig.textFontFamily
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }

        MouseArea {
            anchors.fill: parent
            onClicked: button.clicked()
        }
    }
}
