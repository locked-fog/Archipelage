import QtQuick
import ArchipelagoBackend

Item {
    id: root

    property int compactLevel: 0
    property string moduleId: ""
    property var handlers: ({})

    // The media plugin owns its own state loader. Framework code
    // (IslandModule, IslandHost) only sees a Component reference; it
    // never has to know that media needs an internal MediaState singleton.
    Loader {
        id: mediaStateLoader
        anchors.fill: parent
        active: true
        sourceComponent: mediaStateComponent
        visible: false
    }

    Component {
        id: mediaStateComponent

        MediaState {}
    }

    readonly property var mediaState: mediaStateLoader.item

    Component.onCompleted: {
        // secondary click → toggle play/pause (when user has
        // configured that as the secondary action for "media").
        // wheel → next / previous track. Returning false from a
        // handler makes the framework fall through to its default
        // (host.activateModule → open the expanded surface).
        handlers = {
            secondaryClicked: function(action) {
                if (action !== "playPause")
                    return false;
                if (mediaState && mediaState.activePlayer) {
                    mediaState.togglePlaying();
                    return true;
                }
                return false;
            },
            wheelMoved: function(delta) {
                if (!mediaState || !mediaState.activePlayer)
                    return false;
                if (delta > 0 && mediaState.activePlayer.previous) {
                    mediaState.activePlayer.previous();
                    return true;
                }
                if (delta < 0 && mediaState.activePlayer.next) {
                    mediaState.activePlayer.next();
                    return true;
                }
                return false;
            }
        };
    }

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
