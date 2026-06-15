import QtQuick
import ArchipelagoBackend

IslandCapsule {
    id: root

    property string appName: ""
    property string summary: ""
    property string body: ""

    signal openRequested()

    visible: opacity > 0
    opacity: notificationTimer.running ? 1 : 0
    interactive: notificationTimer.running

    function show(nextAppName, nextSummary, nextBody) {
        appName = nextAppName || "";
        summary = nextSummary || "";
        body = nextBody || "";
        notificationTimer.restart();
    }

    onPrimaryClicked: root.openRequested()
    onSecondaryClicked: notificationTimer.stop()

    Timer {
        id: notificationTimer

        interval: 4200
        repeat: false
    }

    Row {
        anchors.fill: parent
        spacing: 9

        Text {
            text: "N"
            color: StyleTokens.accent
            font.pixelSize: 14
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.Bold
            anchors.verticalCenter: parent.verticalCenter
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 28
            spacing: 0

            Text {
                width: parent.width
                text: root.summary !== "" ? root.summary : root.appName
                color: StyleTokens.textPrimary
                font.pixelSize: 12
                font.family: ArchipelagoConfig.textFontFamily
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                width: parent.width
                text: root.body
                color: StyleTokens.textSecondary
                font.pixelSize: 11
                font.family: ArchipelagoConfig.textFontFamily
                elide: Text.ElideRight
                maximumLineCount: 1
                visible: root.body !== ""
            }
        }
    }
}
