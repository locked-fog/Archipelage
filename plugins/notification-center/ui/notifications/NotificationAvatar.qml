import QtQuick
import ArchipelagoCore

Item {
    id: root

    property var notification: ({})
    property string fallbackIcon: "message"
    property color backgroundColor: StyleTokens.track
    property color borderColor: Qt.rgba(1, 1, 1, 0.08)
    property color strokeColor: StyleTokens.textPrimary

    readonly property string imageSource: notification && notification.imageSource !== undefined
        ? String(notification.imageSource || "")
        : ""
    readonly property string appName: notification && notification.appName !== undefined
        ? String(notification.appName || "")
        : ""
    readonly property bool imageReady: imageSource !== "" && avatarImage.status === Image.Ready
    readonly property string initials: appName.trim().length > 0 ? appName.trim().charAt(0).toUpperCase() : ""

    Rectangle {
        anchors.fill: parent
        radius: Math.min(width, height) / 2
        color: root.backgroundColor
        border.width: 1
        border.color: root.borderColor
        clip: true

        Image {
            id: avatarImage

            anchors.fill: parent
            anchors.margins: 1
            source: root.imageSource
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            visible: root.imageReady
            sourceSize.width: width
            sourceSize.height: height
        }

        Text {
            anchors.centerIn: parent
            visible: !root.imageReady && root.initials !== ""
            text: root.initials
            textFormat: Text.PlainText
            color: StyleTokens.textPrimary
            font.family: ArchipelagoConfig.textFontFamily
            font.pixelSize: Math.max(11, Math.round(parent.width * 0.42))
            font.weight: Font.DemiBold
        }

        NotificationGlyph {
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.5)
            height: width
            visible: !root.imageReady && root.initials === ""
            icon: root.fallbackIcon
            strokeColor: root.strokeColor
            accentColor: StyleTokens.warning
        }
    }
}
