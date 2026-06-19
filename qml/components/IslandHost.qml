import QtQuick
import ArchipelagoCore

Item {
    id: root

    property string expandedModule: ""
    property var shellWindow: null
    property int topMargin: 4
    property int expandedGeometryRevision: 0
    readonly property var anchorsConfig: ArchipelagoConfig.anchors
    readonly property var leftActiveModules: modulesForAnchor("left")
    readonly property var centerActiveModules: modulesForAnchor("center")
    readonly property var rightActiveModules: modulesForAnchor("right")
    property var leftRenderedModules: []
    property var centerRenderedModules: []
    property var rightRenderedModules: []
    readonly property int compactLevel: width < 1180 ? 2 : (width < 1480 ? 1 : 0)
    readonly property real groupY: topMargin
    readonly property real groupHeight: ArchipelagoConfig.islandHeight
    readonly property var layoutState: computeLayoutState()
    readonly property real leftX: anchorMetric("left", "x")
    readonly property real leftWidth: anchorMetric("left", "width")
    readonly property real centerX: anchorMetric("center", "x")
    readonly property real centerWidth: anchorMetric("center", "width")
    readonly property real rightX: anchorMetric("right", "x")
    readonly property real rightWidth: anchorMetric("right", "width")
    readonly property bool expandedMounted: calculateExpandedMask("mounted", expandedGeometryRevision) > 0
    readonly property bool expandedOpened: calculateExpandedMask("opened", expandedGeometryRevision) > 0
    readonly property bool expandedHovered: calculateExpandedMask("hovered", expandedGeometryRevision) > 0
    readonly property real expandedMaskX: calculateExpandedMask("x", expandedGeometryRevision)
    readonly property real expandedMaskY: calculateExpandedMask("y", expandedGeometryRevision)
    readonly property real expandedMaskWidth: calculateExpandedMask("width", expandedGeometryRevision)
    readonly property real expandedMaskHeight: calculateExpandedMask("height", expandedGeometryRevision)

    signal moduleTriggered(string moduleId, var originRect)

    Component.onCompleted: {
        reconcileRenderedModules("left");
        reconcileRenderedModules("center");
        reconcileRenderedModules("right");
    }

    onLeftActiveModulesChanged: reconcileRenderedModules("left")
    onCenterActiveModulesChanged: reconcileRenderedModules("center")
    onRightActiveModulesChanged: reconcileRenderedModules("right")

    IslandLayoutScheduler {
        id: layoutScheduler

        hostWidth: root.width
        compactLevel: root.compactLevel
    }

    Timer {
        id: exitPruneTimer

        interval: ArchipelagoConfig.compactFadeDuration * 3 + 80
        repeat: false

        onTriggered: root.pruneInactiveRenderedModules()
    }

    function modulesForAnchor(anchorName) {
        const list = anchorsConfig[anchorName];
        const configured = [];
        if (list !== undefined && list !== null) {
            for (let index = 0; index < list.length; index++)
                configured.push(list[index]);
        }
        const configuredIds = configuredModuleIds();
        const entries = shellWindow && shellWindow.moduleRegistry ? shellWindow.moduleRegistry : {};
        const manifestDefaults = [];
        const ids = Object.keys(entries);
        for (let index = 0; index < ids.length; index++) {
            const id = ids[index];
            const entry = entries[id] || {};
            const anchors = entry.anchors || [];
            if (configuredIds[id] || anchors.indexOf(anchorName) < 0)
                continue;
            const compactLayout = entry.compactLayout || {};
            manifestDefaults.push({
                id: id,
                priority: compactLayout.priority !== undefined
                    ? Number(compactLayout.priority || 0)
                    : Number(entry.defaultPriority || 0)
            });
        }
        manifestDefaults.sort((a, b) => b.priority - a.priority);
        for (let index = 0; index < manifestDefaults.length; index++)
            configured.push(manifestDefaults[index].id);
        return configured;
    }

    function configuredModuleIds() {
        const result = {};
        const anchors = Object.keys(anchorsConfig || {});
        for (let anchorIndex = 0; anchorIndex < anchors.length; anchorIndex++) {
            const list = anchorsConfig[anchors[anchorIndex]];
            if (!list)
                continue;
            for (let itemIndex = 0; itemIndex < list.length; itemIndex++)
                result[list[itemIndex]] = true;
        }
        return result;
    }

    function moduleConfig(moduleId) {
        const config = ArchipelagoConfig.moduleConfig(moduleId);
        return config === undefined || config === null ? ({}) : config;
    }

    function modulePriority(moduleId) {
        const config = moduleConfig(moduleId);
        if (config.priority !== undefined)
            return Number(config.priority || 0);
        const entry = shellWindow && shellWindow.moduleEntry ? shellWindow.moduleEntry(moduleId) : {};
        const compactLayout = entry && entry.compactLayout ? entry.compactLayout : {};
        if (compactLayout.priority !== undefined)
            return Number(compactLayout.priority || 0);
        return Number(entry.defaultPriority || 0);
    }

    function shouldShowModule(moduleId) {
        if (!ArchipelagoConfig.moduleEnabled(moduleId))
            return false;
        if (compactLevel >= 1 && modulePriority(moduleId) < 35)
            return false;
        return true;
    }

    function compactFor(moduleId) {
        return shellWindow && shellWindow.compactComponentFor ? shellWindow.compactComponentFor(moduleId) : null;
    }

    function expandedFor(moduleId) {
        return shellWindow && shellWindow.expandedComponentFor ? shellWindow.expandedComponentFor(moduleId) : null;
    }

    function layoutFor(moduleId) {
        const byModule = layoutState.byModule || {};
        return byModule[moduleId] || {
            "moduleId": moduleId,
            "visible": false,
            "targetWidth": 0,
            "targetX": 0,
            "targetOpacity": 0,
            "collapseReason": "space-pressure",
            "anchor": ""
        };
    }

    function activeModulesForAnchor(anchorName) {
        if (anchorName === "left")
            return leftActiveModules;
        if (anchorName === "center")
            return centerActiveModules;
        if (anchorName === "right")
            return rightActiveModules;
        return [];
    }

    function renderedModulesForAnchor(anchorName) {
        if (anchorName === "left")
            return leftRenderedModules;
        if (anchorName === "center")
            return centerRenderedModules;
        if (anchorName === "right")
            return rightRenderedModules;
        return [];
    }

    function setRenderedModulesForAnchor(anchorName, modules) {
        if (anchorName === "left")
            leftRenderedModules = modules;
        else if (anchorName === "center")
            centerRenderedModules = modules;
        else if (anchorName === "right")
            rightRenderedModules = modules;
    }

    function isSameModuleList(left, right) {
        if (left.length !== right.length)
            return false;
        for (let index = 0; index < left.length; index++) {
            if (left[index] !== right[index])
                return false;
        }
        return true;
    }

    function isModuleActive(anchorName, moduleId) {
        const active = activeModulesForAnchor(anchorName);
        return active.indexOf(moduleId) >= 0;
    }

    function reconcileRenderedModules(anchorName) {
        const active = activeModulesForAnchor(anchorName);
        const rendered = renderedModulesForAnchor(anchorName);
        const next = [];
        let hasExiting = false;
        for (let index = 0; index < active.length; index++) {
            const moduleId = active[index];
            if (next.indexOf(moduleId) < 0)
                next.push(moduleId);
        }
        for (let index = 0; index < rendered.length; index++) {
            const moduleId = rendered[index];
            if (active.indexOf(moduleId) < 0 && next.indexOf(moduleId) < 0) {
                next.push(moduleId);
                hasExiting = true;
            }
        }
        if (!isSameModuleList(rendered, next))
            setRenderedModulesForAnchor(anchorName, next);
        if (hasExiting)
            exitPruneTimer.restart();
    }

    function noteModuleExitFinished(moduleId, anchorName) {
        if (isModuleActive(anchorName, moduleId))
            return;
        const rendered = renderedModulesForAnchor(anchorName);
        const next = [];
        for (let index = 0; index < rendered.length; index++) {
            if (rendered[index] !== moduleId)
                next.push(rendered[index]);
        }
        if (!isSameModuleList(rendered, next))
            setRenderedModulesForAnchor(anchorName, next);
    }

    function pruneInactiveRenderedModules() {
        pruneInactiveRenderedModulesForAnchor("left");
        pruneInactiveRenderedModulesForAnchor("center");
        pruneInactiveRenderedModulesForAnchor("right");
    }

    function pruneInactiveRenderedModulesForAnchor(anchorName) {
        const rendered = renderedModulesForAnchor(anchorName);
        const next = [];
        for (let index = 0; index < rendered.length; index++) {
            const moduleId = rendered[index];
            if (isModuleActive(anchorName, moduleId))
                next.push(moduleId);
        }
        if (!isSameModuleList(rendered, next))
            setRenderedModulesForAnchor(anchorName, next);
    }

    function activateModule(moduleId, item) {
        if (!item)
            return;
        const point = item.mapToItem(root.parent, 0, 0);
        moduleTriggered(moduleId, {
            "x": point.x,
            "y": point.y,
            "width": item.width,
            "height": item.height
        });
    }

    function activateModuleById(moduleId) {
        const rect = originRectForModule(moduleId);
        if (rect !== null)
            moduleTriggered(moduleId, rect);
    }

    function moduleItems() {
        const result = [];
        collectModuleItems(root, result);
        return result;
    }

    function collectModuleItems(item, result) {
        const children = item && item.children ? item.children : [];
        for (let index = 0; index < children.length; index++) {
            const child = children[index];
            if (child && child.moduleId !== undefined && child.anchorName !== undefined)
                result.push(child);
            collectModuleItems(child, result);
        }
    }

    function anchorModuleItems(anchorName) {
        const items = moduleItems();
        const result = [];
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            if (item && item.anchorName === anchorName)
                result.push(item);
        }
        return result;
    }

    function moduleItem(moduleId) {
        const items = moduleItems();
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            if (item && item.moduleId === moduleId && item.layoutVisible)
                return item;
        }
        return null;
    }

    function anchorMetric(anchorName, field) {
        const items = anchorModuleItems(anchorName);
        let minX = 1000000;
        let maxX = 0;
        let hasVisible = false;
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            if (!item || !item.visible || item.width <= 0.5)
                continue;
            hasVisible = true;
            minX = Math.min(minX, item.x);
            maxX = Math.max(maxX, item.x + item.width);
        }
        if (!hasVisible)
            return 0;
        if (field === "x")
            return minX;
        return maxX - minX;
    }

    function openExpandedModule(moduleId, originRect) {
        const target = moduleItem(moduleId);
        if (!target)
            return;
        const items = moduleItems();
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            if (item && item !== target)
                item.closeExpanded();
        }
        target.openExpanded(originRect);
    }

    function closeExpanded() {
        const items = moduleItems();
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            if (item)
                item.closeExpanded();
        }
        bumpExpandedGeometry();
    }

    function noteModuleOpening(moduleId) {
        expandedModule = moduleId;
        bumpExpandedGeometry();
    }

    function noteModuleClosed(moduleId) {
        if (expandedModule === moduleId)
            expandedModule = "";
        bumpExpandedGeometry();
    }

    function bumpExpandedGeometry() {
        expandedGeometryRevision = expandedGeometryRevision + 1;
    }

    function calculateExpandedMask(field, revision) {
        const items = moduleItems();
        if (field === "mounted" || field === "opened" || field === "hovered") {
            for (let index = 0; index < items.length; index++) {
                const item = items[index];
                if (!item)
                    continue;
                if (field === "mounted" && item.expandedMounted)
                    return 1;
                if (field === "opened" && item.expandedOpened)
                    return 1;
                if (field === "hovered" && item.expandedHovered)
                    return 1;
            }
            return 0;
        }

        let hasSurface = false;
        let minX = 1000000;
        let minY = 1000000;
        let maxX = 0;
        let maxY = 0;
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            if (!item || !item.expandedMounted)
                continue;
            hasSurface = true;
            minX = Math.min(minX, item.expandedMaskX);
            minY = Math.min(minY, item.expandedMaskY);
            maxX = Math.max(maxX, item.expandedMaskX + item.expandedMaskWidth);
            maxY = Math.max(maxY, item.expandedMaskY + item.expandedMaskHeight);
        }
        if (!hasSurface)
            return 0;
        if (field === "x")
            return minX;
        if (field === "y")
            return minY;
        if (field === "width")
            return maxX - minX;
        return maxY - minY;
    }

    function originRectForModule(moduleId) {
        const target = moduleItem(moduleId);
        if (!target)
            return null;
        const point = target.mapToItem(root.parent, 0, 0);
        return {
            "x": point.x,
            "y": point.y,
            "width": target.width,
            "height": target.height
        };
    }

    function computeLayoutState() {
        return layoutScheduler.computeLayoutState(anchorModuleItems("left"),
                                                  anchorModuleItems("center"),
                                                  anchorModuleItems("right"));
    }

    IslandAnchorRow {
        anchors.fill: parent
        anchorName: "left"
        host: root
        modules: root.leftRenderedModules
        compactLevel: root.compactLevel
        topMargin: root.topMargin

        onModuleExitFinished: function(moduleId, anchorName) {
            root.noteModuleExitFinished(moduleId, anchorName);
        }
    }

    IslandAnchorRow {
        anchors.fill: parent
        anchorName: "center"
        host: root
        modules: root.centerRenderedModules
        compactLevel: root.compactLevel
        topMargin: root.topMargin

        onModuleExitFinished: function(moduleId, anchorName) {
            root.noteModuleExitFinished(moduleId, anchorName);
        }
    }

    IslandAnchorRow {
        anchors.fill: parent
        anchorName: "right"
        host: root
        modules: root.rightRenderedModules
        compactLevel: root.compactLevel
        topMargin: root.topMargin

        onModuleExitFinished: function(moduleId, anchorName) {
            root.noteModuleExitFinished(moduleId, anchorName);
        }
    }
}
