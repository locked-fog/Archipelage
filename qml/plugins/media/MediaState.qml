import QtQuick
import Quickshell.Services.Mpris
import ArchipelagoBackend

Item {
    id: root

    visible: false
    width: 0
    height: 0

    property bool expanded: false
    property string lastActivePlayerDbusName: ""
    property var playersList: Mpris.players.values !== undefined ? Mpris.players.values : Mpris.players
    property var activePlayer: resolveActivePlayer()
    property real trackProgress: 0
    property string timePlayed: "0:00"
    property string timeTotal: "0:00"

    readonly property string currentTrack: activePlayer ? (activePlayer.trackTitle || activePlayer.title || "Unknown") : ""
    readonly property string currentArtist: {
        if (!activePlayer)
            return "";
        const artist = activePlayer.artist || (activePlayer.metadata ? activePlayer.metadata["xesam:artist"] : "");
        if (Array.isArray(artist))
            return artist.join(", ");
        return artist ? String(artist) : "Unknown";
    }
    readonly property string currentArtUrl: activePlayer ? (activePlayer.trackArtUrl || activePlayer.artUrl || "") : ""
    readonly property bool isPlaying: activePlayer && activePlayer.playbackState === MprisPlaybackState.Playing
    readonly property string displayText: {
        if (!activePlayer)
            return "No media";
        const lyric = SysBackend && SysBackend.lyricsIsSynced && SysBackend.lyricsCurrentLyric
            ? SysBackend.lyricsCurrentLyric
            : "";
        if (lyric !== "")
            return lyric;
        return currentTrack || "Media";
    }

    onActivePlayerChanged: {
        Qt.callLater(function() {
            const nextDbusName = root.activePlayer && root.activePlayer.dbusName
                ? root.activePlayer.dbusName
                : "";
            if (root.lastActivePlayerDbusName !== nextDbusName)
                root.lastActivePlayerDbusName = nextDbusName;
        });
    }

    function playerHasTrackInfo(player) {
        if (!player)
            return false;
        if ((player.trackTitle || player.title || "") !== "")
            return true;
        if (!player.metadata)
            return false;
        return Boolean(
            player.metadata["xesam:title"]
            || player.metadata["mpris:trackid"]
            || player.metadata["xesam:url"]
        );
    }

    function findPlayerByDbusName(dbusName) {
        if (!playersList || !dbusName)
            return null;
        for (let index = 0; index < playersList.length; index++) {
            if (playersList[index].dbusName === dbusName)
                return playersList[index];
        }
        return null;
    }

    function resolveActivePlayer() {
        if (!playersList || playersList.length === 0)
            return null;

        for (let index = 0; index < playersList.length; index++) {
            if (playersList[index].playbackState === MprisPlaybackState.Playing)
                return playersList[index];
        }

        const rememberedPlayer = findPlayerByDbusName(lastActivePlayerDbusName);
        if (rememberedPlayer && (playerHasTrackInfo(rememberedPlayer) || rememberedPlayer.canControl))
            return rememberedPlayer;

        for (let index = 0; index < playersList.length; index++) {
            if (playersList[index].playbackState === MprisPlaybackState.Paused && playerHasTrackInfo(playersList[index]))
                return playersList[index];
        }

        for (let index = 0; index < playersList.length; index++) {
            if (playersList[index].canControl)
                return playersList[index];
        }

        return playersList[0];
    }

    function formatTime(value) {
        const numberValue = Number(value);
        if (isNaN(numberValue) || numberValue <= 0)
            return "0:00";

        let totalSeconds = 0;
        if (numberValue < 10000)
            totalSeconds = Math.floor(numberValue);
        else if (numberValue < 100000000)
            totalSeconds = Math.floor(numberValue / 1000);
        else
            totalSeconds = Math.floor(numberValue / 1000000);

        const minutes = Math.floor(totalSeconds / 60);
        const seconds = Math.floor(totalSeconds % 60);
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
    }

    function togglePlaying() {
        if (!activePlayer || !activePlayer.canControl)
            return;
        if (activePlayer.canTogglePlaying) {
            activePlayer.togglePlaying();
            return;
        }
        if (activePlayer.playbackState === MprisPlaybackState.Playing) {
            if (activePlayer.canPause)
                activePlayer.pause();
            return;
        }
        if (activePlayer.canPlay)
            activePlayer.play();
    }

    Timer {
        interval: 500
        running: root.activePlayer !== null && root.expanded
        repeat: true

        onTriggered: {
            const player = root.activePlayer;
            if (!player)
                return;

            const currentPosition = Number(player.position) || 0;
            let totalLength = Number(player.length) || 0;
            if (totalLength <= 0 && player.metadata && player.metadata["mpris:length"])
                totalLength = Number(player.metadata["mpris:length"]);

            if (totalLength > 0) {
                root.trackProgress = currentPosition / totalLength;
                root.timePlayed = root.formatTime(currentPosition);
                root.timeTotal = root.formatTime(totalLength);
            } else {
                root.trackProgress = 0;
                root.timePlayed = root.formatTime(currentPosition);
                root.timeTotal = "0:00";
            }
        }
    }
}
