import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property int compactLevel: 0
    property string moduleId: ""
    property var handlers: ({})
    property date now: new Date()

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }

    Row {
        anchors.centerIn: parent
        spacing: 7

        Rectangle {
            width: 7
            height: 7
            radius: 4
            color: StyleTokens.danger
            visible: SystemServices.screenRecordingActive
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: Qt.formatTime(root.now, root.compactLevel >= 2 ? "HH:mm" : "HH:mm:ss")
            color: StyleTokens.textPrimary
            font.pixelSize: root.compactLevel >= 2 ? 14 : 15
            font.family: ArchipelagoConfig.timeFontFamily
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
