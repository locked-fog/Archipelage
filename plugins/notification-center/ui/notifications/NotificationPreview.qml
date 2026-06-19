import QtQuick
import ArchipelagoCore

Item {
    id: root

    property var payload: ({})
    property string instanceId: ""
    property var previewController: null
    property var shellWindow: null

    Row {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        NotificationAvatar {
            width: 42
            height: 42
            anchors.verticalCenter: parent.verticalCenter
            notification: root.payload
            fallbackIcon: "message"
            backgroundColor: Qt.rgba(0.39, 0.78, 1.0, 0.16)
            borderColor: Qt.rgba(0.39, 0.78, 1.0, 0.32)
            strokeColor: StyleTokens.textPrimary
        }

        Column {
            width: parent.width - 54
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Text {
                width: parent.width
                text: payload.appName || "Notification"
                textFormat: Text.PlainText
                color: StyleTokens.textSecondary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 11
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                width: parent.width
                text: payload.summary || payload.body || ""
                textFormat: Text.PlainText
                color: StyleTokens.textPrimary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                width: parent.width
                text: payload.body || ""
                textFormat: Text.PlainText
                color: StyleTokens.textSecondary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 12
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                visible: text !== ""
            }
        }
    }
}
