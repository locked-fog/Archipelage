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

        Rectangle {
            width: 42
            height: 42
            radius: 21
            anchors.verticalCenter: parent.verticalCenter
            color: Qt.rgba(0.39, 0.78, 1.0, 0.16)
            border.width: 1
            border.color: Qt.rgba(0.39, 0.78, 1.0, 0.32)

            NotificationGlyph {
                anchors.centerIn: parent
                width: 20
                height: 20
                icon: "message"
                strokeColor: StyleTokens.textPrimary
            }
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
