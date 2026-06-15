import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property int compactLevel: 0
    property var mediaState: null

    Row {
        anchors.fill: parent
        spacing: 8

        Text {
            text: mediaState && mediaState.isPlaying ? ">" : "||"
            color: mediaState && mediaState.activePlayer ? StyleTokens.textPrimary : StyleTokens.textSecondary
            font.pixelSize: 13
            font.family: ArchipelagoConfig.textFontFamily
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 24
            text: mediaState ? mediaState.displayText : "No media"
            color: mediaState && mediaState.activePlayer ? StyleTokens.textPrimary : StyleTokens.textSecondary
            font.pixelSize: 13
            font.family: ArchipelagoConfig.textFontFamily
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
