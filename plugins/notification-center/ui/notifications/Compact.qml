import QtQuick
import ArchipelagoCore
import ArchipelagoPlugins.Notifications 1.0

Item {
    id: root

    property int compactLevel: 0
    property string moduleId: "notifications"
    property var shellWindow: null
    property int activeNotificationId: 0
    property bool transientVisible: false
    property string activePreviewId: ""
    readonly property string clientId: "compact:" + moduleId
    readonly property int compactLayoutPriority: 85
    readonly property bool compactVisibleRequested: true
    readonly property int collapsedWidth: compactLevel >= 2 ? 42 : 46
    readonly property int expandedWidth: compactLevel >= 2 ? 232 : 282
    readonly property int minimumCompactWidth: collapsedWidth
    readonly property int maximumCompactWidth: 340
    readonly property bool lowPreviewVisible: NotificationCenterService.mode === "low"
        && transientVisible
        && activeNotificationId > 0
    readonly property bool dndMessageVisible: NotificationCenterService.mode === "dnd"
        && transientVisible
        && activeNotificationId > 0
    readonly property bool actionWindowActive: lowPreviewVisible || dndMessageVisible
    readonly property int preferredCompactWidth: lowPreviewVisible ? expandedWidth : collapsedWidth
    readonly property var activeNotification: activeNotificationId > 0
        ? NotificationCenterService.notification(activeNotificationId)
        : NotificationCenterService.latestNotification
    readonly property int badgeCount: NotificationCenterService.unreadCount
    readonly property string glyphName: actionWindowActive ? "message" : NotificationCenterService.mode

    property var handlers: ({
        "primaryClicked": function() {
            if (!root.actionWindowActive)
                return false

            const item = root.activeNotification && root.activeNotification.id !== undefined
                ? root.activeNotification
                : NotificationCenterService.latestNotification
            const id = Number(item.id || 0)
            if (id <= 0)
                return false

            root.dismissActivePreview()
            root.transientVisible = false
            root.activeNotificationId = 0

            return NotificationCenterService.invokeNotification(id, "default")
        },
        "secondaryClicked": function() {
            root.dismissActivePreview()
            root.transientVisible = false
            root.activeNotificationId = 0
            collapseTimer.stop()
            return true
        }
    })

    Component.onCompleted: NotificationCenterService.registerClient(clientId)
    Component.onDestruction: NotificationCenterService.releaseClient(clientId)

    function dismissActivePreview() {
        if (activePreviewId !== "" && shellWindow && shellWindow.dismissPreview)
            shellWindow.dismissPreview(activePreviewId)
        activePreviewId = ""
    }

    function activatePayload(payload) {
        const id = Number(payload && payload.id !== undefined ? payload.id : 0)
        if (id <= 0)
            return
        const invoked = NotificationCenterService.invokeNotification(id, "default")
        activePreviewId = ""
        if (!invoked && shellWindow && shellWindow.openExpanded)
            shellWindow.openExpanded(moduleId, shellWindow.originRectForModule(moduleId))
    }

    function showNormalPreview(notification) {
        if (!shellWindow || !shellWindow.showPreview)
            return
        activePreviewId = shellWindow.showPreview(moduleId, "message", notification, {
            "timeoutMs": notification.expireTimeout > 0 ? notification.expireTimeout : 5000,
            "activateOnPrimary": true,
            "onActivated": function(payload) {
                root.activatePayload(payload)
            },
            "onDismissed": function(instanceId) {
                if (root.activePreviewId === instanceId)
                    root.activePreviewId = ""
            }
        })
    }

    Connections {
        target: NotificationCenterService

        function onNotificationArrived(notification) {
            root.activeNotificationId = Number(notification.id || 0)
            if (NotificationCenterService.mode === "normal") {
                root.transientVisible = false
                root.showNormalPreview(notification)
                return
            }

            root.dismissActivePreview()
            root.transientVisible = true
            collapseTimer.restart()
        }
    }

    Timer {
        id: collapseTimer

        interval: 5000
        repeat: false
        onTriggered: {
            root.transientVisible = false
            root.activeNotificationId = 0
        }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 2
        anchors.rightMargin: 4
        spacing: lowPreviewVisible ? 9 : 0

        Item {
            width: root.collapsedWidth - 8
            height: parent.height

            NotificationAvatar {
                width: compactLevel >= 2 ? 17 : 19
                height: width
                anchors.centerIn: parent
                notification: root.actionWindowActive ? root.activeNotification : ({})
                fallbackIcon: root.glyphName
                backgroundColor: "transparent"
                borderColor: "transparent"
                strokeColor: root.dndMessageVisible ? StyleTokens.warning : StyleTokens.textPrimary
            }

            Rectangle {
                visible: root.badgeCount > 0
                width: Math.max(15, countLabel.implicitWidth + 7)
                height: 15
                radius: 8
                anchors.right: parent.right
                anchors.rightMargin: -2
                anchors.top: parent.top
                anchors.topMargin: 3
                color: StyleTokens.danger

                Text {
                    id: countLabel

                    anchors.centerIn: parent
                    text: root.badgeCount > 99 ? "99+" : String(root.badgeCount)
                    color: StyleTokens.white
                    font.family: ArchipelagoConfig.textFontFamily
                    font.pixelSize: root.badgeCount > 99 ? 8 : 9
                    font.weight: Font.DemiBold
                }
            }
        }

        Column {
            width: Math.max(0, parent.width - root.collapsedWidth - 12)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            opacity: root.lowPreviewVisible ? 1 : 0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutCubic
                }
            }

            Text {
                width: parent.width
                text: root.activeNotification.appName || "Notification"
                textFormat: Text.PlainText
                color: StyleTokens.textSecondary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: 10
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                width: parent.width
                text: root.activeNotification.summary || root.activeNotification.body || ""
                textFormat: Text.PlainText
                color: StyleTokens.textPrimary
                font.family: ArchipelagoConfig.textFontFamily
                font.pixelSize: compactLevel >= 2 ? 11 : 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }
}
