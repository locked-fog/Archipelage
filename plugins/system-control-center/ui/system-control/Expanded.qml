import QtQuick
import QtQuick.Layouts
import ArchipelagoCore
import ArchipelagoPlugins.SystemControl 1.0

Item {
    id: root

    property string moduleId: "system-control"
    property var shellWindow: null
    readonly property string clientId: "expanded:" + moduleId
    readonly property int controlRowCount: 4
    readonly property real rowSpacing: 12
    readonly property real cellSpacing: 12
    readonly property real sideButtonWidth: width > 460 ? 134 : 120
    property string activeSecondaryPreview: ""

    function dismissActivePreview() {
        if (activeSecondaryPreview === "")
            return
        activeSecondaryPreview = ""
        if (shellWindow && shellWindow.dismissAllPreviews)
            shellWindow.dismissAllPreviews()
    }

    function previewFrom(item, templateId) {
        if (!shellWindow)
            return
        activeSecondaryPreview = templateId
        shellWindow.showPreview(moduleId, templateId, {}, {
            "originRect": shellWindow.originRectForItem(item),
            "onDismissed": function(instanceId, pluginId, closedTemplateId) {
                if (root.activeSecondaryPreview === closedTemplateId)
                    root.activeSecondaryPreview = ""
            }
        })
    }

    function outputSummary() {
        if (AudioRouteService.activeOutputName !== "")
            return AudioRouteService.activeOutputName.replace(/^.*\./, "").replace(/[-_]/g, " ")
        return "Select output"
    }

    function displaySummary() {
        if (DisplayService.activeDeviceId !== "")
            return DisplayService.activeDeviceId.replace(/_/g, " ")
        return "Choose display"
    }

    Component.onCompleted: {
        ConnectivityService.registerClient(clientId)
        BluetoothService.registerClient(clientId)
        AudioRouteService.registerClient(clientId)
        DisplayService.registerClient(clientId)
        PowerProfileService.registerClient(clientId)
    }

    Component.onDestruction: {
        ConnectivityService.releaseClient(clientId)
        BluetoothService.releaseClient(clientId)
        AudioRouteService.releaseClient(clientId)
        DisplayService.releaseClient(clientId)
        PowerProfileService.releaseClient(clientId)
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 18
        clip: true
        contentWidth: width
        contentHeight: contentColumn.implicitHeight

        Column {
            id: contentColumn

            width: parent.width
            spacing: root.rowSpacing

            Row {
                width: parent.width
                spacing: root.cellSpacing

                QuickSettingCard {
                    id: wifiCard

                    width: Math.floor((parent.width - root.cellSpacing) / 2)
                    height: 122
                    title: "Wi-Fi"
                    subtitle: ConnectivityService.lastError !== ""
                        ? ConnectivityService.lastError
                        : ConnectivityService.statusText
                    active: ConnectivityService.wifiEnabled || ConnectivityService.wiredConnected
                    warning: ConnectivityService.lastError !== ""
                    previewVisible: root.activeSecondaryPreview === "wifi"
                    onClicked: ConnectivityService.setWifiEnabled(!ConnectivityService.wifiEnabled)
                    onLongPressed: root.previewFrom(cardSurface, "wifi")

                    glyph: Component {
                        NetworkGlyph {
                            width: 24
                            height: 24
                            wiredConnected: ConnectivityService.wiredConnected
                            wifiEnabled: ConnectivityService.wifiEnabled
                            wifiConnected: ConnectivityService.wifiConnected
                            signalStrength: ConnectivityService.activeSignal
                            searchPhase: 2
                            activeColor: StyleTokens.textPrimary
                            inactiveColor: StyleTokens.textSecondary
                        }
                    }
                }

                QuickSettingCard {
                    id: bluetoothCard

                    width: Math.floor((parent.width - root.cellSpacing) / 2)
                    height: 122
                    title: "Bluetooth"
                    subtitle: BluetoothService.lastError !== ""
                        ? BluetoothService.lastError
                        : BluetoothService.statusText
                    active: BluetoothService.powered
                    warning: BluetoothService.lastError !== ""
                    previewVisible: root.activeSecondaryPreview === "bluetooth"
                    onClicked: BluetoothService.setPowered(!BluetoothService.powered)
                    onLongPressed: root.previewFrom(cardSurface, "bluetooth")

                    glyph: Component {
                        SystemGlyph {
                            width: 24
                            height: 24
                            icon: "bluetooth"
                            color: StyleTokens.textPrimary
                        }
                    }
                }
            }

            Row {
                width: parent.width
                spacing: root.cellSpacing

                SliderSurface {
                    width: parent.width - root.sideButtonWidth - root.cellSpacing
                    height: 104
                    title: "Volume"
                    subtitle: AudioRouteService.muted ? "Muted" : (Math.round(AudioRouteService.volume * 100) + "%")

                    glyph: Component {
                        VolumeGlyph {
                            width: 22
                            height: 22
                            volume: AudioRouteService.volume
                            muted: AudioRouteService.muted
                        }
                    }

                    sliderContent: Component {
                        ValueSlider {
                            width: parent.width
                            value: Math.max(0, Math.min(1, AudioRouteService.volume))
                            onMoved: function(nextValue) {
                                AudioRouteService.setVolume(nextValue)
                            }
                        }
                    }
                }

                ActionButton {
                    id: audioButton

                    width: root.sideButtonWidth
                    height: 104
                    title: "Mixer"
                    subtitle: "Outputs"
                    previewVisible: root.activeSecondaryPreview === "audio-output"
                    onClicked: root.previewFrom(audioButton, "audio-output")

                    glyph: Component {
                        SystemGlyph {
                            width: 22
                            height: 22
                            icon: "speaker"
                            color: StyleTokens.textPrimary
                        }
                    }
                }
            }

            Row {
                width: parent.width
                spacing: root.cellSpacing

                SliderSurface {
                    width: parent.width - root.sideButtonWidth - root.cellSpacing
                    height: 104
                    title: "Brightness"
                    subtitle: Math.round(DisplayService.brightness * 100) + "%"

                    glyph: Component {
                        BrightnessGlyph {
                            width: 22
                            height: 22
                            brightness: DisplayService.brightness
                        }
                    }

                    sliderContent: Component {
                        ValueSlider {
                            width: parent.width
                            value: Math.max(0, Math.min(1, DisplayService.brightness))
                            onMoved: function(nextValue) {
                                DisplayService.setBrightness(nextValue)
                            }
                        }
                    }
                }

                ActionButton {
                    id: displayButton

                    width: root.sideButtonWidth
                    height: 104
                    title: "Displays"
                    subtitle: root.displaySummary()
                    previewVisible: root.activeSecondaryPreview === "display"
                    onClicked: root.previewFrom(displayButton, "display")

                    glyph: Component {
                        SystemGlyph {
                            width: 22
                            height: 22
                            icon: "sun"
                            color: StyleTokens.textPrimary
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                objectName: "powerModeCard"
                implicitHeight: Math.max(86, powerColumn.implicitHeight + 28)
                height: implicitHeight
                radius: 24
                color: StyleTokens.module
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)

                Column {
                    id: powerColumn

                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Row {
                        spacing: 10

                        SystemGlyph {
                            width: 18
                            height: 18
                            icon: "bolt"
                            color: StyleTokens.textPrimary
                        }

                        Text {
                            text: PowerProfileService.lastError !== ""
                                ? PowerProfileService.lastError
                                : (PowerProfileService.available
                                    ? "Power mode"
                                    : "Power profiles unavailable")
                            color: PowerProfileService.lastError !== "" ? StyleTokens.danger : StyleTokens.textPrimary
                            font.family: ArchipelagoConfig.textFontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }

                    Flow {
                        id: powerRow

                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: PowerProfileService.profiles

                            ProfileChip {
                                required property var modelData
                                label: modelData.label
                                profileId: modelData.id
                                chipWidth: Math.max(88, Math.floor((powerRow.width - powerRow.spacing * 2) / 3))
                            }
                        }
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.activeSecondaryPreview !== ""
        visible: enabled
        z: 80
        acceptedButtons: Qt.LeftButton
        onClicked: root.dismissActivePreview()
    }

    component QuickSettingCard: Rectangle {
        id: card

        property alias cardSurface: card
        property string title: ""
        property string subtitle: ""
        property bool active: false
        property bool warning: false
        property bool previewVisible: false
        property Component glyph
        property int holdMs: 380
        property bool holdTriggered: false
        signal clicked()
        signal longPressed()
        objectName: title !== "" ? "quickSettingCard:" + title : "quickSettingCard"

        radius: 24
        opacity: previewVisible ? 0 : 1
        color: warning
            ? Qt.rgba(1.0, 0.43, 0.43, 0.16)
            : (active ? Qt.rgba(0.39, 0.78, 1.0, 0.18) : StyleTokens.track)
        border.width: active || warning ? 1 : 0
        border.color: warning ? StyleTokens.danger : Qt.rgba(0.39, 0.78, 1.0, 0.34)

        Behavior on opacity {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Row {
                spacing: 10

                Rectangle {
                    width: 46
                    height: 46
                    radius: 23
                    color: card.active ? Qt.rgba(1, 1, 1, 0.14) : StyleTokens.module

                    Loader {
                        anchors.centerIn: parent
                        sourceComponent: card.glyph
                    }
                }

                Column {
                    width: card.width - 84
                    spacing: 4

                    Text {
                        text: card.title
                        color: StyleTokens.textPrimary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Text {
                        width: parent.width
                        text: card.subtitle
                        color: warning ? StyleTokens.danger : StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: !card.previewVisible
            acceptedButtons: Qt.LeftButton
            onPressed: {
                card.holdTriggered = false
                holdTimer.restart()
            }
            onReleased: {
                holdTimer.stop()
                if (!card.holdTriggered)
                    card.clicked()
                card.holdTriggered = false
            }
            onCanceled: {
                card.holdTriggered = false
                holdTimer.stop()
            }
        }

        Timer {
            id: holdTimer
            interval: card.holdMs
            repeat: false
            onTriggered: {
                card.holdTriggered = true
                card.longPressed()
            }
        }
    }

    component SliderSurface: Rectangle {
        id: surface

        property string title: ""
        property string subtitle: ""
        property Component glyph
        property Component sliderContent

        radius: 24
        color: StyleTokens.module
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            Row {
                spacing: 10

                Rectangle {
                    width: 42
                    height: 42
                    radius: 21
                    color: StyleTokens.track

                    Loader {
                        anchors.centerIn: parent
                        sourceComponent: surface.glyph
                    }
                }

                Column {
                    width: surface.width - 80
                    spacing: 3

                    Text {
                        text: surface.title
                        color: StyleTokens.textPrimary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: surface.subtitle
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 12
                    }
                }
            }

            Loader {
                width: parent.width
                sourceComponent: surface.sliderContent
            }
        }
    }

    component ActionButton: Rectangle {
        id: button

        property string title: ""
        property string subtitle: ""
        property bool previewVisible: false
        property Component glyph
        signal clicked()

        radius: 24
        opacity: previewVisible ? 0 : 1
        color: StyleTokens.module
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)
        objectName: title !== "" ? title.toLowerCase() + "Button" : "actionButton"

        Behavior on opacity {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Rectangle {
                width: 42
                height: 42
                radius: 21
                color: StyleTokens.track

                Loader {
                    anchors.centerIn: parent
                    sourceComponent: button.glyph
                }
            }

            Text {
                text: button.title
                color: StyleTokens.textPrimary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            Text {
                width: parent.width
                text: button.subtitle
                color: StyleTokens.textSecondary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: !button.previewVisible
            onClicked: button.clicked()
        }
    }

    component ValueSlider: Item {
        id: slider

        property real value: 0
        signal moved(real nextValue)

        function clamp(nextValue) {
            return Math.max(0, Math.min(1, nextValue))
        }

        function setFromX(x) {
            moved(clamp(width <= 0 ? 0 : x / width))
        }

        width: 300
        height: 28

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: 8
            radius: 4
            color: StyleTokens.track
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * slider.clamp(slider.value)
            height: 8
            radius: 4
            color: StyleTokens.accent
        }

        Rectangle {
            x: parent.width * slider.clamp(slider.value) - width / 2
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            radius: 9
            color: StyleTokens.textPrimary
            border.width: 2
            border.color: StyleTokens.accent
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: function(mouse) {
                slider.setFromX(mouse.x)
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    slider.setFromX(mouse.x)
            }
        }
    }

    component ProfileChip: Rectangle {
        id: profileChip

        property string label: ""
        property string profileId: ""
        property real chipWidth: 0

        width: chipWidth > 0 ? chipWidth : Math.max(92, labelNode.implicitWidth + 24)
        height: 34
        radius: 17
        color: PowerProfileService.activeProfile === profileId
            ? Qt.rgba(0.39, 0.78, 1.0, 0.18)
            : StyleTokens.track
        border.width: PowerProfileService.activeProfile === profileId ? 1 : 0
        border.color: Qt.rgba(0.39, 0.78, 1.0, 0.32)

        Text {
            id: labelNode
            anchors.centerIn: parent
            text: profileChip.label
            color: StyleTokens.textPrimary
            font.family: ArchipelagoConfig.textFontFamily
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }

        MouseArea {
            anchors.fill: parent
            onClicked: PowerProfileService.setProfile(profileChip.profileId)
        }
    }
}
