import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property var entries: []

    Connections {
        target: SystemServices

        function onNotificationReceived(appName, summary, body) {
            const next = entries.slice(0, 9);
            next.unshift({
                "appName": appName,
                "summary": summary,
                "body": body
            });
            entries = next;
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 20

        Text {
            id: title

            text: "Notifications"
            color: StyleTokens.textPrimary
            font.pixelSize: ArchipelagoConfig.titleFontSize
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
        }

        Flickable {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: title.bottom
            anchors.topMargin: 16
            anchors.bottom: parent.bottom
            clip: true
            contentHeight: listColumn.height

            Column {
                id: listColumn

                width: parent.width
                spacing: 8

                Repeater {
                    model: root.entries

                    Rectangle {
                        required property var modelData

                        width: listColumn.width
                        height: 58
                        radius: 20
                        color: StyleTokens.module

                        Column {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            anchors.topMargin: 9
                            spacing: 1

                            Text {
                                width: parent.width
                                text: modelData.summary || modelData.appName || "Notification"
                                color: StyleTokens.textPrimary
                                font.pixelSize: 13
                                font.family: ArchipelagoConfig.textFontFamily
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }

                            Text {
                                width: parent.width
                                text: modelData.body || ""
                                color: StyleTokens.textSecondary
                                font.pixelSize: 12
                                font.family: ArchipelagoConfig.textFontFamily
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }
                    }
                }

                Text {
                    width: parent.width
                    text: "No notifications"
                    color: StyleTokens.textSecondary
                    font.pixelSize: 13
                    font.family: ArchipelagoConfig.textFontFamily
                    horizontalAlignment: Text.AlignHCenter
                    visible: root.entries.length === 0
                }
            }
        }
    }
}
