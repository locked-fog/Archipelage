import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property date now: new Date()
    readonly property int year: now.getFullYear()
    readonly property int month: now.getMonth()
    readonly property int today: now.getDate()

    function daysInMonth(yearValue, monthValue) {
        return new Date(yearValue, monthValue + 1, 0).getDate();
    }

    function firstDayOffset(yearValue, monthValue) {
        return new Date(yearValue, monthValue, 1).getDay();
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }

    Item {
        anchors.fill: parent
        anchors.margins: 20

        Text {
            id: timeText

            text: Qt.formatTime(root.now, "HH:mm:ss")
            color: StyleTokens.textPrimary
            font.pixelSize: 42
            font.family: ArchipelagoConfig.timeFontFamily
            font.weight: Font.DemiBold
        }

        Text {
            anchors.left: timeText.left
            anchors.top: timeText.bottom
            anchors.topMargin: 2
            text: Qt.formatDate(root.now, "dddd, MMMM d")
            color: StyleTokens.textSecondary
            font.pixelSize: 14
            font.family: ArchipelagoConfig.textFontFamily
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            width: 122
            height: 34
            radius: 17
            color: SystemServices.screenRecordingActive ? StyleTokens.danger : StyleTokens.module

            Text {
                anchors.centerIn: parent
                text: SystemServices.screenRecordingActive ? "Recording" : "Idle"
                color: StyleTokens.textPrimary
                font.pixelSize: 12
                font.family: ArchipelagoConfig.textFontFamily
                font.weight: Font.DemiBold
            }
        }

        Grid {
            id: calendar

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 210
            columns: 7
            rowSpacing: 6
            columnSpacing: 6

            Repeater {
                model: 42

                Rectangle {
                    required property int index

                    readonly property int dayNumber: index - root.firstDayOffset(root.year, root.month) + 1
                    readonly property bool inMonth: dayNumber >= 1 && dayNumber <= root.daysInMonth(root.year, root.month)
                    width: (calendar.width - calendar.columnSpacing * 6) / 7
                    height: 30
                    radius: 15
                    color: inMonth && dayNumber === root.today ? StyleTokens.accent : StyleTokens.transparent

                    Text {
                        anchors.centerIn: parent
                        text: parent.inMonth ? String(parent.dayNumber) : ""
                        color: parent.inMonth
                            ? (parent.dayNumber === root.today ? StyleTokens.white : StyleTokens.textPrimary)
                            : StyleTokens.textSecondary
                        font.pixelSize: 12
                        font.family: ArchipelagoConfig.textFontFamily
                    }
                }
            }
        }
    }
}
