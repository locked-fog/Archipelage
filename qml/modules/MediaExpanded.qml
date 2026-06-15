import QtQuick
import ArchipelagoBackend

Item {
    id: root

    MediaState {
        id: mediaState
        expanded: true
    }

    function togglePlaying() {
        mediaState.togglePlaying();
    }

    Item {
        anchors.fill: parent
        anchors.margins: 20

        Rectangle {
            id: art

            anchors.left: parent.left
            anchors.top: parent.top
            width: 132
            height: 132
            radius: 28
            color: StyleTokens.module
            clip: true

            Image {
                anchors.fill: parent
                source: mediaState.currentArtUrl
                fillMode: Image.PreserveAspectCrop
                visible: mediaState.currentArtUrl !== ""
            }

            Text {
                anchors.centerIn: parent
                text: "MEDIA"
                color: StyleTokens.textSecondary
                font.pixelSize: 12
                font.family: ArchipelagoConfig.textFontFamily
                font.weight: Font.DemiBold
                visible: mediaState.currentArtUrl === ""
            }
        }

        Column {
            anchors.left: art.right
            anchors.leftMargin: 18
            anchors.right: parent.right
            anchors.top: art.top
            spacing: 6

            Text {
                width: parent.width
                text: mediaState.currentTrack || "No media playing"
                color: StyleTokens.textPrimary
                font.pixelSize: 20
                font.family: ArchipelagoConfig.textFontFamily
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: mediaState.currentArtist
                color: StyleTokens.textSecondary
                font.pixelSize: 13
                font.family: ArchipelagoConfig.textFontFamily
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                width: parent.width
                text: mediaState.displayText
                color: StyleTokens.textMuted
                font.pixelSize: 13
                font.family: ArchipelagoConfig.textFontFamily
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        Rectangle {
            id: progressTrack

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: art.bottom
            anchors.topMargin: 28
            height: 10
            radius: 5
            color: StyleTokens.track
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * Math.max(0, Math.min(1, mediaState.trackProgress))
                radius: parent.radius
                color: StyleTokens.textPrimary
            }
        }

        Row {
            anchors.left: progressTrack.left
            anchors.right: progressTrack.right
            anchors.top: progressTrack.bottom
            anchors.topMargin: 8

            Text {
                text: mediaState.timePlayed
                color: StyleTokens.textSecondary
                font.pixelSize: 11
                font.family: ArchipelagoConfig.textFontFamily
            }

            Item { width: parent.width - 90; height: 1 }

            Text {
                text: mediaState.timeTotal
                color: StyleTokens.textSecondary
                font.pixelSize: 11
                font.family: ArchipelagoConfig.textFontFamily
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            spacing: 26

            MediaButton {
                label: "Prev"
                enabled: mediaState.activePlayer !== null
                onClicked: if (mediaState.activePlayer) mediaState.activePlayer.previous()
            }

            MediaButton {
                label: mediaState.isPlaying ? "Pause" : "Play"
                emphasized: true
                enabled: mediaState.activePlayer !== null
                onClicked: root.togglePlaying()
            }

            MediaButton {
                label: "Next"
                enabled: mediaState.activePlayer !== null
                onClicked: if (mediaState.activePlayer) mediaState.activePlayer.next()
            }
        }
    }
}
