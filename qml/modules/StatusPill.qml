import QtQuick
import ArchipelagoBackend

Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property color accent: StyleTokens.accent

    height: 66
    radius: StyleTokens.radiusModule
    color: StyleTokens.module

    Column {
        anchors.centerIn: parent
        width: parent.width - 18
        spacing: 4

        Text {
            width: parent.width
            text: root.label
            color: StyleTokens.textSecondary
            font.pixelSize: 11
            font.family: ArchipelagoConfig.textFontFamily
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: root.value
            color: root.accent
            font.pixelSize: 13
            font.family: ArchipelagoConfig.textFontFamily
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }
}
