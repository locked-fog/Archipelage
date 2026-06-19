import QtQuick
import ArchipelagoCore
import ArchipelagoPlugins.Notifications 1.0

Item {
    id: root

    property string moduleId: "notifications"
    property var shellWindow: null
    readonly property string clientId: "expanded:" + moduleId
    readonly property var allNotifications: NotificationCenterService.notifications
    readonly property int unreadCount: NotificationCenterService.unreadCount
    readonly property int rowSpacing: 8

    Component.onCompleted: NotificationCenterService.registerClient(clientId)
    Component.onDestruction: NotificationCenterService.releaseClient(clientId)

    function modeLabel(mode) {
        if (mode === "low")
            return "Low"
        if (mode === "dnd")
            return "DND"
        return "Normal"
    }

    function timeText(createdAt) {
        const value = Number(createdAt || 0)
        if (value <= 0)
            return ""
        const date = new Date(value)
        return Qt.formatTime(date, "HH:mm")
    }

    function invokeNotification(id, actionKey) {
        const invoked = NotificationCenterService.invokeNotification(Number(id || 0), actionKey || "default")
        if (!invoked)
            NotificationCenterService.markRead(Number(id || 0))
    }

    Item {
        anchors.fill: parent
        anchors.margins: 20

        Row {
            id: header

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 44
            spacing: 10

            Column {
                width: Math.max(120, parent.width - clearButton.width - modeButton.width - 26)
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    text: "Notifications"
                    color: StyleTokens.textPrimary
                    font.family: ArchipelagoConfig.textFontFamily
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Text {
                    text: root.unreadCount + " unread"
                    color: root.unreadCount > 0 ? StyleTokens.accent : StyleTokens.textSecondary
                    font.family: ArchipelagoConfig.textFontFamily
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }

            HeaderButton {
                id: clearButton
                objectName: "clearButton"
                label: "Clear"
                enabled: root.allNotifications.length > 0
                onClicked: NotificationCenterService.clearAll()
            }

            HeaderButton {
                id: modeButton
                objectName: "modeButton"
                label: root.modeLabel(NotificationCenterService.mode)
                accent: NotificationCenterService.mode !== "normal"
                onClicked: NotificationCenterService.cycleMode()
            }
        }

        Rectangle {
            id: statusStrip

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.topMargin: 14
            height: NotificationCenterService.lastError === "" ? 0 : 34
            radius: 17
            color: Qt.rgba(1.0, 0.23, 0.19, 0.16)
            border.width: height > 0 ? 1 : 0
            border.color: Qt.rgba(1.0, 0.23, 0.19, 0.32)
            visible: height > 0

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                text: NotificationCenterService.lastError
                textFormat: Text.PlainText
                color: StyleTokens.danger
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 11
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Flickable {
            id: listView

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: statusStrip.visible ? statusStrip.bottom : header.bottom
            anchors.topMargin: 14
            anchors.bottom: parent.bottom
            clip: true
            contentHeight: listColumn.height

            Column {
                id: listColumn

                width: listView.width
                spacing: root.rowSpacing

                Repeater {
                    model: root.allNotifications

                    NotificationRow {
                        width: listColumn.width
                        notification: modelData
                        visible: notification.unread === true
                        height: visible ? 86 : 0
                        onInvoked: function(actionKey) {
                            root.invokeNotification(notification.id, actionKey)
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: root.unreadCount === 0 ? 170 : 0
                    visible: height > 0

                    Column {
                        anchors.centerIn: parent
                        spacing: 10

                        Rectangle {
                            width: 54
                            height: 54
                            radius: 27
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: StyleTokens.module

                            NotificationGlyph {
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                icon: NotificationCenterService.mode
                                strokeColor: StyleTokens.textSecondary
                            }
                        }

                        Text {
                            text: "No unread notifications"
                            color: StyleTokens.textSecondary
                            font.family: ArchipelagoConfig.textFontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }
    }

    component HeaderButton: Rectangle {
        id: button

        property string label: ""
        property bool accent: false
        signal clicked()

        width: Math.max(70, labelNode.implicitWidth + 24)
        height: 34
        radius: 17
        color: !enabled ? StyleTokens.track : (accent ? Qt.rgba(0.39, 0.78, 1.0, 0.18) : StyleTokens.module)
        border.width: accent ? 1 : 0
        border.color: Qt.rgba(0.39, 0.78, 1.0, 0.32)
        opacity: enabled ? 1 : 0.45

        Text {
            id: labelNode
            anchors.centerIn: parent
            text: button.label
            color: StyleTokens.textPrimary
            font.family: ArchipelagoConfig.textFontFamily
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }

        MouseArea {
            anchors.fill: parent
            enabled: button.enabled
            acceptedButtons: Qt.LeftButton
            onClicked: button.clicked()
        }
    }

    component NotificationRow: Rectangle {
        id: row

        property var notification: ({})
        signal invoked(string actionKey)

        function actionButtonCount() {
            const items = notification.actions || []
            let count = 0
            for (let index = 0; index < items.length; index++) {
                if (items[index].key !== "default")
                    count = count + 1
            }
            return count
        }

        radius: 22
        color: StyleTokens.module
        border.width: 1
        border.color: notification.urgency >= 2 ? Qt.rgba(1, 0.23, 0.19, 0.40) : Qt.rgba(1, 1, 1, 0.08)
        clip: true

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: row.invoked("default")
        }

        Row {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            NotificationAvatar {
                width: 40
                height: 40
                anchors.verticalCenter: parent.verticalCenter
                notification: row.notification
                fallbackIcon: "message"
                backgroundColor: notification.urgency >= 2 ? Qt.rgba(1, 0.23, 0.19, 0.16) : StyleTokens.track
                borderColor: notification.urgency >= 2 ? Qt.rgba(1, 0.23, 0.19, 0.34) : Qt.rgba(1, 1, 1, 0.08)
                strokeColor: notification.urgency >= 2 ? StyleTokens.danger : StyleTokens.textPrimary
            }

            Column {
                width: Math.max(0, parent.width - 52 - actionColumn.width - 14)
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Row {
                    width: parent.width

                    Text {
                        width: Math.max(0, parent.width - timeLabel.width - 10)
                        text: notification.appName || "Notification"
                        textFormat: Text.PlainText
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Text {
                        id: timeLabel
                        text: root.timeText(notification.createdAt)
                        color: StyleTokens.textSecondary
                        font.family: ArchipelagoConfig.textFontFamily
                        font.pixelSize: 11
                    }
                }

                Text {
                    width: parent.width
                    text: notification.summary || ""
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
                    text: notification.body || ""
                    textFormat: Text.PlainText
                    color: StyleTokens.textSecondary
                    font.family: ArchipelagoConfig.textFontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    visible: text !== ""
                }
            }

            Column {
                id: actionColumn

                width: row.actionButtonCount() > 0 ? 92 : 0
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                visible: width > 0

                Repeater {
                    id: actionRepeater
                    model: notification.actions || []

                    ActionChip {
                        visible: modelData.key !== "default"
                        width: visible ? actionColumn.width : 0
                        height: visible ? 26 : 0
                        label: modelData.label || modelData.key
                        onClicked: row.invoked(modelData.key)
                    }
                }
            }
        }
    }

    component ActionChip: Rectangle {
        id: chip

        property string label: ""
        signal clicked()

        radius: 13
        color: StyleTokens.track
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)

        Text {
            anchors.centerIn: parent
            width: parent.width - 12
            text: chip.label
            textFormat: Text.PlainText
            color: StyleTokens.textPrimary
            font.family: ArchipelagoConfig.textFontFamily
            font.pixelSize: 11
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: chip.clicked()
        }
    }
}
