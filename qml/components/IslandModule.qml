import QtQuick
import ArchipelagoCore

// IslandModule is the framework's per-capsule container. It owns no
// per-moduleId business logic — the active plugin's Compact.qml is
// responsible for its own primaryClicked / secondaryClicked /
// wheelMoved behaviour, exposed via the `handlers` property.
//
// Wire protocol with a plugin Compact.qml:
//   property int    compactLevel     (framework-injected)
//   property string moduleId         (framework-injected; null-safe)
//   property var    shellWindow      (framework-injected, optional)
//   property int    preferredCompactWidth   (plugin-set, optional)
//   property int    minimumCompactWidth     (plugin-set, optional)
//   property int    maximumCompactWidth     (plugin-set, optional)
//   property bool   compactVisibleRequested (plugin-set, optional)
//   property int    compactLayoutPriority   (plugin-set, optional)
//   property var    handlers         (plugin-set, optional)
//       handlers.primaryClicked()                  -> bool
//       handlers.secondaryClicked(action: string)   -> bool
//       handlers.wheelMoved(delta: int)             -> void
//       handlers.pointerPressed(localX, localY, button, buttons)  -> void
//       handlers.pointerMoved(localX, localY, buttons)            -> void
//       handlers.pointerReleased(localX, localY, button, buttons) -> void
//       handlers.pointerCanceled()                      -> void
//
// Returning false from a handler tells the framework to apply its
// default behaviour (typically host.activateModule → open expanded).
Item {
    id: root

    required property string moduleId
    property string anchorName: ""
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
    readonly property var rawModuleConfig: ArchipelagoConfig.moduleConfig(moduleId)
    readonly property var moduleConfig: rawModuleConfig ? rawModuleConfig : ({})
    readonly property var moduleEntry: root.host && root.host.shellWindow && root.host.shellWindow.moduleEntry
        ? root.host.shellWindow.moduleEntry(root.moduleId)
        : ({})
    readonly property var manifestCompactLayout: moduleEntry && moduleEntry.compactLayout
        ? moduleEntry.compactLayout
        : ({})
    readonly property int configuredWidth: moduleConfig.width !== undefined
        ? Number(moduleConfig.width || 0)
        : 0
    readonly property var moduleRegistrySnapshot: root.host && root.host.shellWindow
        ? root.host.shellWindow.moduleRegistry
        : ({})
    readonly property var assignedLayout: root.host && root.host.layoutFor
        ? root.host.layoutFor(root.moduleId)
        : ({
              "visible": true,
              "targetWidth": configuredWidth > 0 ? configuredWidth : 120,
              "targetOpacity": 1,
              "collapseReason": ""
          })
    readonly property bool layoutVisible: assignedLayout.visible !== false
    readonly property real targetLayoutOpacity: assignedLayout.targetOpacity !== undefined
        ? Number(assignedLayout.targetOpacity || 0)
        : 1
    readonly property var layoutRequest: ({
            "moduleId": root.moduleId,
            "anchor": root.anchorName,
            "preferredWidth": root.effectivePreferredWidth(),
            "minimumWidth": root.effectiveMinimumWidth(),
            "maximumWidth": root.effectiveMaximumWidth(),
            "visibleRequested": root.effectiveVisibleRequested(),
            "priority": root.effectivePriority()
        })

    width: assignedLayout.targetWidth !== undefined
        ? Number(assignedLayout.targetWidth || 0)
        : (configuredWidth > 0 ? configuredWidth : 120)
    height: ArchipelagoConfig.islandHeight
    opacity: (compactVisible ? 1 : 0) * targetLayoutOpacity
    visible: layoutVisible || opacity > 0.01 || width > 0.5 || expandedSurface.mounted
    enabled: layoutVisible && !expanded && !expandedSurface.mounted

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

    function compactPointerPoint(x, y) {
        const item = compactLoader.item;
        if (!item || !capsule)
            return {
                "x": x,
                "y": y
            };
        return capsule.mapToItem(item, x, y);
    }

    function hasCompactProperty(name) {
        const item = compactLoader.item;
        return !!item && item[name] !== undefined;
    }

    function compactNumberProperty(name, fallback) {
        const item = compactLoader.item;
        if (!item || item[name] === undefined || item[name] === null)
            return fallback;
        const value = Number(item[name]);
        return isFinite(value) ? Math.round(value) : fallback;
    }

    function compactBoolProperty(name, fallback) {
        const item = compactLoader.item;
        if (!item || item[name] === undefined || item[name] === null)
            return fallback;
        return item[name] === true;
    }

    function effectivePreferredWidth() {
        if (configuredWidth > 0)
            return configuredWidth;
        if (hasCompactProperty("preferredCompactWidth"))
            return Math.max(0, compactNumberProperty("preferredCompactWidth", 0));
        return Math.max(0, Number(manifestCompactLayout.preferredWidth || 0));
    }

    function effectiveMinimumWidth() {
        if (hasCompactProperty("minimumCompactWidth"))
            return Math.max(0, compactNumberProperty("minimumCompactWidth", 44));
        const manifestValue = Number(manifestCompactLayout.minimumWidth || 0);
        return manifestValue > 0 ? manifestValue : 44;
    }

    function effectiveMaximumWidth() {
        if (hasCompactProperty("maximumCompactWidth"))
            return Math.max(0, compactNumberProperty("maximumCompactWidth", 360));
        const manifestValue = Number(manifestCompactLayout.maximumWidth || 0);
        return manifestValue > 0 ? manifestValue : 360;
    }

    function effectiveVisibleRequested() {
        if (hasCompactProperty("compactVisibleRequested"))
            return compactBoolProperty("compactVisibleRequested", true);
        if (manifestCompactLayout.visible !== undefined)
            return manifestCompactLayout.visible !== false;
        return true;
    }

    function effectivePriority() {
        if (moduleConfig.priority !== undefined)
            return Number(moduleConfig.priority || 0);
        if (hasCompactProperty("compactLayoutPriority"))
            return compactNumberProperty("compactLayoutPriority", 0);
        if (manifestCompactLayout.priority !== undefined)
            return Number(manifestCompactLayout.priority || 0);
        return Number(moduleEntry.defaultPriority || 0);
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

        onPointerPressed: function(x, y, button, buttons) {
            const point = root.compactPointerPoint(x, y);
            root.invokeHandler("pointerPressed", point.x, point.y, button, buttons);
        }

        onPointerMoved: function(x, y, buttons) {
            const point = root.compactPointerPoint(x, y);
            root.invokeHandler("pointerMoved", point.x, point.y, buttons);
        }

        onPointerReleased: function(x, y, button, buttons) {
            const point = root.compactPointerPoint(x, y);
            root.invokeHandler("pointerReleased", point.x, point.y, button, buttons);
        }

        onPointerCanceled: {
            root.invokeHandler("pointerCanceled");
        }

        Loader {
            id: compactLoader
            anchors.fill: parent
            readonly property var registryDependency: root.moduleRegistrySnapshot

            sourceComponent: registryDependency !== undefined && root.host
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
