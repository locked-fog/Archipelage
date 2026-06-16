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
//   property var    shellWindow      (framework-injected, optional)
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
    readonly property bool expandedMounted: expandedSurface.mounted
    readonly property bool expandedOpened: expandedSurface.opened
    readonly property bool expandedHovered: expandedSurface.hovered
    readonly property real expandedMaskX: expandedSurface.maskX
    readonly property real expandedMaskY: expandedSurface.maskY
    readonly property real expandedMaskWidth: expandedSurface.maskWidth
    readonly property real expandedMaskHeight: expandedSurface.maskHeight
    readonly property bool compactVisible: !expanded || expandedSurface.collapsed
    readonly property var moduleConfig: ArchipelagoConfig.moduleConfig(moduleId)
    readonly property int configuredWidth: Number(moduleConfig.width || 120)

    width: configuredWidth
    height: ArchipelagoConfig.islandHeight
    opacity: compactVisible ? 1 : 0
    enabled: !expanded && !expandedSurface.mounted

    Behavior on opacity {
        enabled: !expandedSurface.collapsed

        NumberAnimation {
            duration: ArchipelagoConfig.compactFadeDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: capsule.fadeCurve
        }
    }

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

    function openExpanded(originRect) {
        expandedSurface.openModule(root.moduleId, originRect);
    }

    function closeExpanded() {
        expandedSurface.close();
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

        Binding {
            target: compactLoader.item
            property: "shellWindow"
            value: root.host ? root.host.shellWindow : null
            when: compactLoader.item && compactLoader.item.shellWindow !== undefined
            restoreMode: Binding.RestoreNone
        }

    }

    ExpandedSurface {
        id: expandedSurface

        parent: root.host ? root.host : root
        shellWindow: root.host ? root.host.shellWindow : null
        z: 40

        onOpening: {
            if (root.host)
                root.host.noteModuleOpening(root.moduleId);
        }

        onClosed: {
            if (root.host)
                root.host.noteModuleClosed(root.moduleId);
        }

        onGeometryChanged: {
            if (root.host)
                root.host.bumpExpandedGeometry();
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
