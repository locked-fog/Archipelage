import QtQuick
import ArchipelagoCore
import ArchipelagoPlugins.SystemControl 1.0

Item {
    id: root

    property var payload: ({})
    property string instanceId: ""
    property var previewController: null
    property var shellWindow: null
    property string activeTab: "mixer"

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Row {
            spacing: 8

            TabButton {
                label: "Mixer"
                active: root.activeTab === "mixer"
                onClicked: root.activeTab = "mixer"
            }

            TabButton {
                label: "Outputs"
                active: root.activeTab === "outputs"
                onClicked: root.activeTab = "outputs"
            }
        }

        Flickable {
            width: parent.width
            height: parent.height - 40
            clip: true
            contentWidth: width
            contentHeight: contentColumn.implicitHeight

            Column {
                id: contentColumn

                width: parent.width
                spacing: 8

                Column {
                    width: parent.width
                    spacing: 8
                    visible: root.activeTab === "mixer"

                    Repeater {
                        model: AudioRouteService.streams

                        Rectangle {
                            required property var modelData

                            width: parent.width
                            height: 74
                            radius: 16
                            color: StyleTokens.module

                            Column {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 10

                                Row {
                                    width: parent.width
                                    spacing: 10

                                    SystemGlyph {
                                        width: 18
                                        height: 18
                                        icon: "speaker"
                                        color: StyleTokens.textPrimary
                                    }

                                    Text {
                                        width: parent.width - 28
                                        text: modelData.name
                                        color: StyleTokens.textPrimary
                                        font.family: ArchipelagoConfig.textFontFamily
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                }

                                PreviewSlider {
                                    width: parent.width
                                    value: Math.max(0, Math.min(1, modelData.volume))
                                    onMoved: function(nextValue) {
                                        AudioRouteService.setStreamVolume(modelData.id, nextValue)
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        visible: AudioRouteService.streams.length === 0
                        text: "No active audio streams."
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 12
                    }
                }

                Column {
                    width: parent.width
                    spacing: 8
                    visible: root.activeTab === "outputs"

                    Repeater {
                        model: AudioRouteService.outputs

                        Rectangle {
                            required property var modelData

                            width: parent.width
                            height: 46
                            radius: 16
                            color: modelData.active ? Qt.rgba(0.39, 0.78, 1.0, 0.18) : StyleTokens.module

                            Row {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 10

                                SystemGlyph {
                                    width: 18
                                    height: 18
                                    icon: "speaker"
                                    color: StyleTokens.textPrimary
                                }

                                Text {
                                    width: parent.width - 84
                                    text: modelData.name
                                    color: StyleTokens.textPrimary
                                    font.family: ArchipelagoConfig.textFontFamily
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                TabButton {
                                    anchors.verticalCenter: parent.verticalCenter
                                    label: modelData.active ? "Current" : "Use"
                                    active: modelData.active
                                    onClicked: {
                                        if (!modelData.active)
                                            AudioRouteService.setDefaultOutput(modelData.id)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component TabButton: Rectangle {
        id: button

        property string label: ""
        property bool active: false
        signal clicked()

        implicitWidth: Math.ceil(labelNode.implicitWidth + 22)
        height: 30
        radius: 15
        color: active ? Qt.rgba(0.39, 0.78, 1.0, 0.18) : StyleTokens.track
        border.width: active ? 1 : 0
        border.color: Qt.rgba(0.39, 0.78, 1.0, 0.32)

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

    component PreviewSlider: Item {
        id: slider

        property real value: 0
        signal moved(real nextValue)

        function clamp(nextValue) {
            return Math.max(0, Math.min(1, nextValue))
        }

        function setFromX(x) {
            moved(clamp(width <= 0 ? 0 : x / width))
        }

        height: 24

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: 6
            radius: 3
            color: StyleTokens.track
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * slider.clamp(slider.value)
            height: 6
            radius: 3
            color: StyleTokens.accent
        }

        Rectangle {
            x: parent.width * slider.clamp(slider.value) - width / 2
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            radius: 7
            color: StyleTokens.textPrimary
        }

        MouseArea {
            anchors.fill: parent
            onPressed: function(mouse) {
                slider.setFromX(mouse.x)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    slider.setFromX(mouse.x)
            }
        }
    }
}
