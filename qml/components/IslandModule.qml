import QtQuick
import ArchipelagoBackend
import "../modules"

Item {
    id: root

    required property string moduleId
    property var host: null
    property int compactLevel: 0
    readonly property bool expanded: host && host.expandedModule === moduleId
    readonly property var moduleConfig: ArchipelagoConfig.moduleConfig(moduleId)
    readonly property int configuredWidth: Number(moduleConfig.width || 120)
    readonly property int compactWidth: {
        if (moduleId === "workspaces")
            return compactLevel >= 2 ? 96 : (compactLevel >= 1 ? 124 : configuredWidth);
        if (moduleId === "clock")
            return compactLevel >= 2 ? 84 : (compactLevel >= 1 ? 96 : configuredWidth);
        if (moduleId === "media")
            return compactLevel >= 1 ? 156 : configuredWidth;
        if (moduleId === "system")
            return compactLevel >= 2 ? 108 : (compactLevel >= 1 ? 128 : configuredWidth);
        return configuredWidth;
    }

    width: compactWidth
    height: ArchipelagoConfig.islandHeight
    opacity: expanded ? 0 : 1
    enabled: !expanded

    function performSecondaryAction() {
        const action = ArchipelagoConfig.moduleAction(moduleId, "secondary");
        const state = mediaStateLoader.item;
        if (moduleId === "media" && action === "playPause" && state && state.activePlayer) {
            state.togglePlaying();
            return;
        }
        if (host)
            host.activateModule(moduleId, root);
    }

    function performWheel(angleDelta) {
        if (moduleId === "workspaces" && Compositor.niriService) {
            Compositor.niriService.focusWorkspaceRelative(angleDelta > 0 ? -1 : 1);
            return;
        }
        const state = mediaStateLoader.item;
        if (moduleId === "media" && state && state.activePlayer) {
            if (angleDelta > 0 && state.activePlayer.previous)
                state.activePlayer.previous();
            else if (angleDelta < 0 && state.activePlayer.next)
                state.activePlayer.next();
        }
    }

    IslandCapsule {
        id: capsule

        anchors.fill: parent
        interactive: root.enabled

        onPrimaryClicked: {
            if (host)
                host.activateModule(root.moduleId, root);
        }
        onSecondaryClicked: root.performSecondaryAction()
        onWheelMoved: function(angleDelta) {
            root.performWheel(angleDelta);
        }

        Loader {
            id: compactLoader
            anchors.fill: parent
            sourceComponent: root.host
                ? (root.host.compactFor(root.moduleId) || fallbackCompact)
                : fallbackCompact

            onLoaded: {
                if (!item)
                    return;
                if (item.compactLevel !== undefined)
                    item.compactLevel = root.compactLevel;
                if (root.moduleId === "media" && item.mediaState !== undefined)
                    item.mediaState = mediaStateLoader.item;
            }
        }
    }

    Loader {
        id: mediaStateLoader

        active: root.moduleId === "media"
        sourceComponent: mediaStateComponent
        visible: false
    }

    Component {
        id: mediaStateComponent

        MediaState {}
    }

    // Fallback used when host.compactFor(moduleId) returns null — e.g.
    // a moduleId present in config but without a registered compact view.
    // Kept inline here because it is framework policy, not plugin-owned.
    Component {
        id: fallbackCompact

        Text {
            anchors.centerIn: parent
            text: root.moduleId
            color: StyleTokens.textPrimary
            font.pixelSize: 13
            font.family: ArchipelagoConfig.textFontFamily
            elide: Text.ElideRight
        }
    }
}
