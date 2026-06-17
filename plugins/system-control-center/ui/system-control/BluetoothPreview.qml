import QtQuick
import ArchipelagoCore
import ArchipelagoPlugins.SystemControl 1.0

Item {
    id: root

    property var payload: ({})
    property string instanceId: ""
    property var previewController: null
    property var shellWindow: null
    readonly property var connectedDevices: BluetoothService.devices.filter(function(entry) {
        return entry.connected
    })
    readonly property var pairedDevices: BluetoothService.devices.filter(function(entry) {
        return entry.paired && !entry.connected
    })

    function openDiscovery(originItem) {
        if (!shellWindow)
            return
        shellWindow.showPreview("system-control", "bluetooth-add", {}, {
            "originRect": shellWindow.originRectForItem(originItem)
        })
    }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Row {
            width: parent.width
            spacing: 8

            Text {
                width: parent.width - addButton.width - 8
                text: "Bluetooth devices"
                color: StyleTokens.textPrimary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            PreviewButton {
                id: addButton
                label: "New device"
                onClicked: root.openDiscovery(addButton)
            }
        }

        Flickable {
            width: parent.width
            height: parent.height - 44
            clip: true
            contentWidth: width
            contentHeight: previewColumn.implicitHeight

            Column {
                id: previewColumn

                width: parent.width
                spacing: 12

                Column {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "Connected"
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: root.connectedDevices

                        DeviceEntry {
                            required property var modelData
                            width: previewColumn.width
                            deviceData: modelData
                            primaryLabel: "Disconnect"
                            onPrimaryClicked: BluetoothService.disconnectDevice(deviceData.address)
                        }
                    }

                    Text {
                        visible: root.connectedDevices.length === 0
                        text: "No connected Bluetooth devices."
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 12
                    }
                }

                Column {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "Paired"
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: root.pairedDevices

                        DeviceEntry {
                            required property var modelData
                            width: previewColumn.width
                            deviceData: modelData
                            primaryLabel: "Connect"
                            onPrimaryClicked: BluetoothService.connectDevice(deviceData.address)
                        }
                    }

                    Text {
                        visible: root.pairedDevices.length === 0
                        text: "No paired devices waiting."
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 12
                    }
                }
            }
        }
    }

    component DeviceEntry: Rectangle {
        id: entryRoot

        property var deviceData: ({})
        property string primaryLabel: ""
        signal primaryClicked()

        height: entryColumn.implicitHeight + 20
        radius: 16
        color: deviceData.connected ? Qt.rgba(0.39, 0.78, 1.0, 0.18) : StyleTokens.module

        Column {
            id: entryColumn

            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Row {
                width: parent.width
                spacing: 10

                SystemGlyph {
                    width: 18
                    height: 18
                    icon: "bluetooth"
                    color: StyleTokens.textPrimary
                }

                Column {
                    width: parent.width - 28
                    spacing: 2

                    Text {
                        width: parent.width
                        text: deviceData.name
                        color: StyleTokens.textPrimary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: deviceData.connected ? "Connected" : "Paired"
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 11
                    }
                }
            }

            PreviewButton {
                label: primaryLabel
                onClicked: entryRoot.primaryClicked()
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
