import QtQuick
import ArchipelagoBackend

// IslandModule is the framework's per-capsule container. It owns no
// per-moduleId business logic — the active plugin's Compact.qml is
// responsible for its own primaryClicked / secondaryClicked /
// wheelMoved behaviour, exposed via the `handlers` property.
//
// Wire protocol with a plugin Compact.qml:
//   property int    compactLevel     (framework-injected)
//   property string moduleId         (framework-injected; null-safe)
//   property var    handlers         (plugin-set, optional)
//       handlers.primaryClicked()                  -> bool
//       handlers.secondaryClicked(action: string)   -> bool
//       handlers.wheelMoved(delta: int)             -> void
//
// Returning false from a handler tells the framework to apply its
// default behaviour (typically host.activateModule → open expanded).
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

    // Per-plugin compactWidth above is the only remaining moduleId
    // branch in the framework; it is a layout hint, not business
    // logic, and is consumed only by the per-row layout. Plugins
    // are free to set their own width inside their Compact.qml too.

    function invokeHandler(name, ...args) {
        const item = compactLoader.item;
        if (!item || !item.handlers)
            return false;
        const fn = item.handlers[name];
        if (typeof fn !== "function")
            return false;
        try {
            return fn.apply(item, args) === true;
        } catch (e) {
            console.warn("[IslandModule] handler", name, "threw:", e);
            return false;
        }
    }

    IslandCapsule {
        id: capsule

        anchors.fill: parent
        interactive: root.enabled

        onPrimaryClicked: {
            if (!root.invokeHandler("primaryClicked") && host)
                host.activateModule(root.moduleId, root);
        }

        onSecondaryClicked: {
            const action = ArchipelagoConfig.moduleAction(root.moduleId, "secondary");
            if (!root.invokeHandler("secondaryClicked", action) && host)
                host.activateModule(root.moduleId, root);
        }

        onWheelMoved: function(angleDelta) {
            root.invokeHandler("wheelMoved", angleDelta);
        }

        Loader {
            id: compactLoader
            anchors.fill: parent
            sourceComponent: root.host
                ? (root.host.compactFor(root.moduleId) || fallbackCompact)
                : fallbackCompact
        }

        Binding {
            target: compactLoader.item
            property: "compactLevel"
            value: root.compactLevel
            when: compactLoader.item && compactLoader.item.compactLevel !== undefined
            restoreMode: Binding.RestoreNone
        }

        Binding {
            target: compactLoader.item
            property: "moduleId"
            value: root.moduleId
            when: compactLoader.item && compactLoader.item.moduleId !== undefined
            restoreMode: Binding.RestoreNone
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
