import QtQuick
import ArchipelagoCore

Item {
    id: root

    property string expandedModule: ""
    property var shellWindow: null
    property int topMargin: 4
    property int expandedGeometryRevision: 0
    readonly property var anchorsConfig: ArchipelagoConfig.anchors
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
        const children = root.children;
        for (let index = 0; index < children.length; index++) {
            const child = children[index];
            if (child && child.moduleId !== undefined && child.anchorName !== undefined)
                result.push(child);
        }
        return result;
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

    function clampWidth(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    function visibleCount(states) {
        let count = 0;
        for (let index = 0; index < states.length; index++) {
            if (states[index].visible)
                count += 1;
        }
        return count;
    }

    function computeRowWidth(states) {
        let total = 0;
        let count = 0;
        for (let index = 0; index < states.length; index++) {
            const state = states[index];
            if (!state.visible)
                continue;
            total += state.currentWidth;
            count += 1;
        }
        if (count > 1)
            total += (count - 1) * ArchipelagoConfig.islandGap;
        return total;
    }

    function prioritizedCandidates(states) {
        const candidates = [];
        for (let index = 0; index < states.length; index++) {
            const state = states[index];
            if (!state.visible)
                continue;
            candidates.push({
                "index": index,
                "priority": state.priority,
                "order": state.order
            });
        }
        candidates.sort((a, b) => {
            if (a.priority !== b.priority)
                return a.priority - b.priority;
            return b.order - a.order;
        });
        return candidates;
    }

    function cloneStates(items) {
        const result = [];
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            result.push({
                "moduleId": item.moduleId,
                "priority": item.priority,
                "order": item.order,
                "minWidth": item.minWidth,
                "currentWidth": item.currentWidth,
                "visible": item.visible,
                "collapseReason": item.collapseReason
            });
        }
        return result;
    }

    function shrinkRowStates(states, budget) {
        if (budget <= 0) {
            for (let index = 0; index < states.length; index++) {
                if (!states[index].visible)
                    continue;
                states[index].visible = false;
                states[index].currentWidth = 0;
                states[index].collapseReason = "space-pressure";
            }
            return states;
        }

        while (computeRowWidth(states) > budget) {
            let changed = false;
            const candidates = prioritizedCandidates(states);
            for (let candidateIndex = 0; candidateIndex < candidates.length; candidateIndex++) {
                const state = states[candidates[candidateIndex].index];
                const reducible = state.currentWidth - state.minWidth;
                if (reducible <= 0)
                    continue;
                const overflow = computeRowWidth(states) - budget;
                const delta = Math.min(reducible, overflow);
                if (delta <= 0)
                    continue;
                state.currentWidth -= delta;
                changed = true;
                if (computeRowWidth(states) <= budget)
                    break;
            }
            if (!changed)
                break;
        }

        while (computeRowWidth(states) > budget) {
            const candidates = prioritizedCandidates(states);
            if (candidates.length === 0)
                break;
            const state = states[candidates[0].index];
            state.visible = false;
            state.currentWidth = 0;
            state.collapseReason = "space-pressure";
        }
        return states;
    }

    function assignRow(anchorName, groupX, states) {
        const assignments = [];
        const byModule = {};
        let x = groupX;
        let groupWidth = 0;
        let visibleSeen = 0;
        for (let index = 0; index < states.length; index++) {
            const state = states[index];
            const visible = state.visible;
            const assignment = {
                "moduleId": state.moduleId,
                "anchor": anchorName,
                "visible": visible,
                "targetWidth": visible ? state.currentWidth : 0,
                "targetX": visible ? x : groupX,
                "targetOpacity": visible ? 1 : 0,
                "collapseReason": visible ? "" : state.collapseReason
            };
            assignments.push(assignment);
            byModule[state.moduleId] = assignment;
            if (!visible)
                continue;
            if (visibleSeen > 0)
                x += ArchipelagoConfig.islandGap;
            assignment.targetX = x;
            x += state.currentWidth;
            visibleSeen += 1;
        }
        if (visibleSeen > 0)
            groupWidth = x - groupX;
        return {
            "groupX": groupX,
            "groupWidth": groupWidth,
            "assignments": assignments,
            "byModule": byModule
        };
    }

    function scheduleRowStates(items, budget) {
        const states = cloneStates(items);
        shrinkRowStates(states, budget);
        return states;
    }

    function scheduleSideRows(leftItems, rightItems, totalBudget) {
        const leftStates = cloneStates(leftItems);
        const rightStates = cloneStates(rightItems);

        function combinedWidth() {
            const leftWidth = computeRowWidth(leftStates);
            const rightWidth = computeRowWidth(rightStates);
            const separator = leftWidth > 0 && rightWidth > 0 ? ArchipelagoConfig.islandGap : 0;
            return leftWidth + rightWidth + separator;
        }

        if (totalBudget <= 0) {
            shrinkRowStates(leftStates, 0);
            shrinkRowStates(rightStates, 0);
        } else {
            while (combinedWidth() > totalBudget) {
                let changed = false;
                const candidates = prioritizedCandidates(leftStates)
                    .map((candidate) => ({
                        "side": "left",
                        "index": candidate.index,
                        "priority": candidate.priority,
                        "order": candidate.order
                    }))
                    .concat(prioritizedCandidates(rightStates).map((candidate) => ({
                        "side": "right",
                        "index": candidate.index,
                        "priority": candidate.priority,
                        "order": candidate.order + leftStates.length
                    })));
                candidates.sort((a, b) => {
                    if (a.priority !== b.priority)
                        return a.priority - b.priority;
                    return b.order - a.order;
                });
                for (let candidateIndex = 0; candidateIndex < candidates.length; candidateIndex++) {
                    const candidate = candidates[candidateIndex];
                    const states = candidate.side === "left" ? leftStates : rightStates;
                    const state = states[candidate.index];
                    const reducible = state.currentWidth - state.minWidth;
                    if (!state.visible || reducible <= 0)
                        continue;
                    const overflow = combinedWidth() - totalBudget;
                    const delta = Math.min(reducible, overflow);
                    if (delta <= 0)
                        continue;
                    state.currentWidth -= delta;
                    changed = true;
                    if (combinedWidth() <= totalBudget)
                        break;
                }
                if (!changed)
                    break;
            }

            while (combinedWidth() > totalBudget) {
                const candidates = prioritizedCandidates(leftStates)
                    .map((candidate) => ({
                        "side": "left",
                        "index": candidate.index,
                        "priority": candidate.priority,
                        "order": candidate.order
                    }))
                    .concat(prioritizedCandidates(rightStates).map((candidate) => ({
                        "side": "right",
                        "index": candidate.index,
                        "priority": candidate.priority,
                        "order": candidate.order + leftStates.length
                    })));
                candidates.sort((a, b) => {
                    if (a.priority !== b.priority)
                        return a.priority - b.priority;
                    return b.order - a.order;
                });
                if (candidates.length === 0)
                    break;
                const candidate = candidates[0];
                const states = candidate.side === "left" ? leftStates : rightStates;
                const state = states[candidate.index];
                state.visible = false;
                state.currentWidth = 0;
                state.collapseReason = "space-pressure";
            }
        }

        const leftLayout = assignRow("left", ArchipelagoConfig.edgeMargin, leftStates);
        const rightWidth = computeRowWidth(rightStates);
        const rightLayout = assignRow("right", width - ArchipelagoConfig.edgeMargin - rightWidth, rightStates);
        return {
            "left": leftLayout,
            "right": rightLayout
        };
    }

    function collectLayoutItems(anchorName) {
        const items = anchorModuleItems(anchorName);
        const result = [];
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            const request = item && item.layoutRequest ? item.layoutRequest : {};
            const preferredWidth = Number(request.preferredWidth || 0);
            const minimumWidth = Math.max(0, Number(request.minimumWidth || 44));
            const maximumWidth = Math.max(minimumWidth, Number(request.maximumWidth || 360));
            const basePreferred = preferredWidth > 0 ? preferredWidth : 120;
            const currentWidth = clampWidth(basePreferred, minimumWidth, maximumWidth);

            let visible = true;
            let collapseReason = "";
            if (!ArchipelagoConfig.moduleEnabled(item.moduleId)) {
                visible = false;
                collapseReason = "disabled";
            } else if (compactLevel >= 1 && Number(request.priority || 0) < 35) {
                visible = false;
                collapseReason = "compact-level";
            } else if (request.visibleRequested === false) {
                visible = false;
                collapseReason = "plugin-hidden";
            }

            result.push({
                "moduleId": item.moduleId,
                "priority": Number(request.priority || 0),
                "order": index,
                "minWidth": minimumWidth,
                "currentWidth": visible ? currentWidth : 0,
                "visible": visible,
                "collapseReason": collapseReason
            });
        }
        return result;
    }

    function mergeAssignments(layoutState, rowState) {
        const next = layoutState;
        const assignments = rowState.assignments || [];
        for (let index = 0; index < assignments.length; index++) {
            const assignment = assignments[index];
            next.byModule[assignment.moduleId] = assignment;
        }
        return next;
    }

    function computeLayoutState() {
        const budget = Math.max(0, width - ArchipelagoConfig.edgeMargin * 2);
        const centerItems = collectLayoutItems("center");
        const centerStates = scheduleRowStates(centerItems, budget);
        const centerWidth = computeRowWidth(centerStates);
        const centerX = centerWidth > 0 ? (width - centerWidth) / 2 : width / 2;
        const centerLayout = assignRow("center", centerX, centerStates);

        const state = {
            "left": {"groupX": ArchipelagoConfig.edgeMargin, "groupWidth": 0, "assignments": []},
            "center": centerLayout,
            "right": {"groupX": width - ArchipelagoConfig.edgeMargin, "groupWidth": 0, "assignments": []},
            "byModule": ({})
        };
        mergeAssignments(state, centerLayout);

        const leftItems = collectLayoutItems("left");
        const rightItems = collectLayoutItems("right");

        if (centerLayout.groupWidth > 0) {
            const leftBudget = Math.max(0, centerLayout.groupX - ArchipelagoConfig.edgeMargin - ArchipelagoConfig.islandGap);
            const centerEnd = centerLayout.groupX + centerLayout.groupWidth;
            const rightBudget = Math.max(0, width - ArchipelagoConfig.edgeMargin - centerEnd - ArchipelagoConfig.islandGap);
            const leftStates = scheduleRowStates(leftItems, leftBudget);
            const rightStates = scheduleRowStates(rightItems, rightBudget);
            state.left = assignRow("left", ArchipelagoConfig.edgeMargin, leftStates);
            state.right = assignRow("right",
                                    width - ArchipelagoConfig.edgeMargin - computeRowWidth(rightStates),
                                    rightStates);
        } else {
            const sideLayouts = scheduleSideRows(leftItems, rightItems, budget);
            state.left = sideLayouts.left;
            state.right = sideLayouts.right;
        }

        mergeAssignments(state, state.left);
        mergeAssignments(state, state.right);
        return state;
    }

    Repeater {
        model: root.modulesForAnchor("left")

        IslandModule {
            required property string modelData

            anchorName: "left"
            moduleId: modelData
            host: root
            compactLevel: root.compactLevel
            x: {
                const layout = root.layoutFor(modelData);
                return layout ? Number(layout.targetX || 0) : 0;
            }
            y: root.topMargin

            Behavior on x {
                NumberAnimation {
                    duration: ArchipelagoConfig.compactFadeDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.33, 1, 0.68, 1, 1, 1]
                }
            }

            Behavior on width {
                NumberAnimation {
                    duration: ArchipelagoConfig.compactFadeDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.33, 1, 0.68, 1, 1, 1]
                }
            }
        }
    }

    Repeater {
        model: root.modulesForAnchor("center")

        IslandModule {
            required property string modelData

            anchorName: "center"
            moduleId: modelData
            host: root
            compactLevel: root.compactLevel
            x: {
                const layout = root.layoutFor(modelData);
                return layout ? Number(layout.targetX || 0) : 0;
            }
            y: root.topMargin

            Behavior on x {
                NumberAnimation {
                    duration: ArchipelagoConfig.compactFadeDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.33, 1, 0.68, 1, 1, 1]
                }
            }

            Behavior on width {
                NumberAnimation {
                    duration: ArchipelagoConfig.compactFadeDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.33, 1, 0.68, 1, 1, 1]
                }
            }
        }
    }

    Repeater {
        model: root.modulesForAnchor("right")

        IslandModule {
            required property string modelData

            anchorName: "right"
            moduleId: modelData
            host: root
            compactLevel: root.compactLevel
            x: {
                const layout = root.layoutFor(modelData);
                return layout ? Number(layout.targetX || 0) : 0;
            }
            y: root.topMargin

            Behavior on x {
                NumberAnimation {
                    duration: ArchipelagoConfig.compactFadeDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.33, 1, 0.68, 1, 1, 1]
                }
            }

            Behavior on width {
                NumberAnimation {
                    duration: ArchipelagoConfig.compactFadeDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.33, 1, 0.68, 1, 1, 1]
                }
            }
        }
    }
}
