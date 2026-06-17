import QtQuick
import ArchipelagoCore
import ArchipelagoPlugins.SystemControl 1.0

Item {
    id: root

    property var payload: ({})
    property string instanceId: ""
    property var previewController: null
    property var shellWindow: null

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Text {
            text: "Displays"
            color: StyleTokens.textPrimary
            font.family: ArchipelagoConfig.textFontFamily
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        Text {
            width: parent.width
            text: DisplayService.lastError !== ""
                ? DisplayService.lastError
                : "Select which backlight device the main brightness slider controls, or adjust each one directly."
            color: DisplayService.lastError !== "" ? StyleTokens.danger : StyleTokens.textSecondary
            wrapMode: Text.WordWrap
            font.family: ArchipelagoConfig.textFontFamily
            font.pixelSize: 12
        }

        Flickable {
            width: parent.width
            height: parent.height - 60
            clip: true
            contentWidth: width
            contentHeight: devicesColumn.implicitHeight

            Column {
                id: devicesColumn

                width: parent.width
                spacing: 8

                Repeater {
                    model: DisplayService.devices

                    Rectangle {
                        required property var modelData

                        width: parent.width
                        height: 86
                        radius: 16
                        color: modelData.active ? Qt.rgba(0.39, 0.78, 1.0, 0.18) : StyleTokens.module

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            Row {
                                width: parent.width
                                spacing: 10

                                BrightnessGlyph {
                                    width: 18
                                    height: 18
                                    brightness: modelData.brightness
                                }

                                Text {
                                    width: parent.width - 96
                                    text: modelData.name
                                    color: StyleTokens.textPrimary
                                    font.family: ArchipelagoConfig.textFontFamily
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                SelectButton {
                                    anchors.verticalCenter: parent.verticalCenter
                                    label: modelData.active ? "Selected" : "Select"
                                    active: modelData.active
                                    onClicked: DisplayService.setActiveDevice(modelData.id)
                                }
                            }

                            PreviewSlider {
                                width: parent.width
                                value: Math.max(0, Math.min(1, modelData.brightness))
                                onMoved: function(nextValue) {
                                    DisplayService.setDeviceBrightness(modelData.id, nextValue)
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: DisplayService.devices.length === 0
                    text: "No backlight devices reported by brightnessctl."
                    color: StyleTokens.textSecondary
                    font.family: ArchipelagoConfig.textFontFamily
                    font.pixelSize: 12
                }
            }
        }
    }

    component SelectButton: Rectangle {
        id: button

        property string label: ""
        property bool active: false
        signal clicked()

        implicitWidth: Math.ceil(labelNode.implicitWidth + 20)
        height: 28
        radius: 14
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
