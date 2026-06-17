import QtQuick
import ArchipelagoCore
import ArchipelagoPlugins.SystemControl 1.0

Item {
    id: root

    property var payload: ({})
    property string instanceId: ""
    property var previewController: null
    property var shellWindow: null
    readonly property var connectedNetworks: ConnectivityService.networks.filter(function(entry) {
        return entry.active
    })
    readonly property var otherNetworks: ConnectivityService.networks.filter(function(entry) {
        return !entry.active
    })

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Row {
            width: parent.width
            spacing: 8

            Text {
                width: parent.width - refreshButton.width - 8
                text: ConnectivityService.wiredConnected
                    ? "Wired active · Wi-Fi networks"
                    : "Wi-Fi networks"
                color: StyleTokens.textPrimary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            PreviewButton {
                id: refreshButton
                label: "Refresh"
                onClicked: ConnectivityService.requestScan()
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
                    visible: root.connectedNetworks.length > 0 || ConnectivityService.wiredConnected

                    Text {
                        text: "Current"
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        width: parent.width
                        height: ConnectivityService.wiredConnected ? 48 : 0
                        radius: 16
                        color: StyleTokens.module
                        visible: ConnectivityService.wiredConnected

                        Row {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            NetworkGlyph {
                                width: 18
                                height: 18
                                wiredConnected: true
                                activeColor: StyleTokens.textPrimary
                                inactiveColor: StyleTokens.textSecondary
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: ConnectivityService.wiredConnectionName !== ""
                                    ? ConnectivityService.wiredConnectionName
                                    : "Ethernet"
                                color: StyleTokens.textPrimary
                                font.family: ArchipelagoConfig.textFontFamily
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Repeater {
                        model: root.connectedNetworks

                        NetworkEntry {
                            required property var modelData
                            width: previewColumn.width
                            networkData: modelData
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "Other networks"
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: root.otherNetworks

                        NetworkEntry {
                            required property var modelData
                            width: previewColumn.width
                            networkData: modelData
                        }
                    }

                    Text {
                        visible: root.otherNetworks.length === 0
                        text: "No additional networks found."
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 12
                    }
                }
            }
        }
    }

    component NetworkEntry: Rectangle {
        id: entryRoot

        property var networkData: ({})

        height: entryColumn.implicitHeight + 20
        radius: 16
        color: networkData.active ? Qt.rgba(0.39, 0.78, 1.0, 0.18) : StyleTokens.module

        Column {
            id: entryColumn

            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Row {
                width: parent.width
                spacing: 10

                NetworkGlyph {
                    width: 18
                    height: 18
                    wifiEnabled: true
                    wifiConnected: networkData.active
                    signalStrength: Math.max(0, networkData.signal)
                    searchPhase: 2
                    activeColor: StyleTokens.textPrimary
                    inactiveColor: StyleTokens.textSecondary
                }

                Column {
                    width: parent.width - 28
                    spacing: 2

                    Text {
                        width: parent.width
                        text: networkData.ssid
                        color: StyleTokens.textPrimary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: networkData.active
                            ? "Connected"
                            : (networkData.available
                                ? (networkData.signal >= 0 ? networkData.signal + "%" : "Saved")
                                : "Saved network")
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }

            Row {
                spacing: 8

                PreviewButton {
                    visible: !networkData.active
                    label: networkData.saved ? "Connect" : "Join"
                    onClicked: ConnectivityService.connectToNetwork(networkData.ssid)
                }

                PreviewButton {
                    visible: networkData.active
                    label: "Disconnect"
                    onClicked: ConnectivityService.disconnectActive()
                }

                PreviewButton {
                    visible: networkData.connectionUuid !== ""
                    label: networkData.autoconnect ? "Auto on" : "Auto off"
                    onClicked: ConnectivityService.setAutoConnect(networkData.connectionUuid, !networkData.autoconnect)
                }

                PreviewButton {
                    visible: networkData.connectionUuid !== ""
                    label: "Forget"
                    onClicked: ConnectivityService.forgetNetwork(networkData.connectionUuid)
                }
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
