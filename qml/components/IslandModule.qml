import QtQuick
import ArchipelagoBackend

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

    // TODO(plugins): per-moduleId branching of secondary / wheel
    // behaviour below is business logic leaking into the framework.
    // Phase 2b will move this into a per-plugin event-handler protocol
    // (each Compact.qml exposes a `handlers` property; IslandModule
    // invokes it without knowing which moduleId is which). For now
    // we still call Compositor / config singletons directly from here.
    function performSecondaryAction() {
        const action = ArchipelagoConfig.moduleAction(moduleId, "secondary");
        if (moduleId === "media" && action === "playPause") {
            const state = mediaStateRef();
            if (state && state.activePlayer) {
                state.togglePlaying();
                return;
            }
        }
        if (host)
            host.activateModule(moduleId, root);
    }

    function performWheel(angleDelta) {
        if (moduleId === "workspaces" && Compositor.niriService) {
            Compositor.niriService.focusWorkspaceRelative(angleDelta > 0 ? -1 : 1);
            return;
        }
        if (moduleId === "media") {
            const state = mediaStateRef();
            if (state && state.activePlayer) {
                if (angleDelta > 0 && state.activePlayer.previous)
                    state.activePlayer.previous();
                else if (angleDelta < 0 && state.activePlayer.next)
                    state.activePlayer.next();
            }
        }
    }

    // Helper: pull the media plugin's MediaState from the loaded
    // compact view. The plugin owns its own MediaState loader; the
    // framework only knows to look at item.mediaState when moduleId
    // is "media". Phase 2b replaces this with a generic handler call.
    function mediaStateRef() {
        const item = compactLoader.item;
        return item && item.mediaState !== undefined ? item.mediaState : null;
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
            }
        }
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
