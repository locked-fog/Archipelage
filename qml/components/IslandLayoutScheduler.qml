import QtQml
import ArchipelagoCore

QtObject {
    id: root

    property real hostWidth: 0
    property int compactLevel: 0

    function clampWidth(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value));
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
        const rightLayout = assignRow("right", hostWidth - ArchipelagoConfig.edgeMargin - rightWidth, rightStates);
        return {
            "left": leftLayout,
            "right": rightLayout
        };
    }

    function collectLayoutItems(items) {
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
            if (request.lifecycleActive === false) {
                visible = false;
                collapseReason = "removed";
            } else if (!ArchipelagoConfig.moduleEnabled(item.moduleId)) {
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

    function computeLayoutState(leftItems, centerItems, rightItems) {
        const budget = Math.max(0, hostWidth - ArchipelagoConfig.edgeMargin * 2);
        const centerStates = scheduleRowStates(collectLayoutItems(centerItems), budget);
        const centerWidth = computeRowWidth(centerStates);
        const centerX = centerWidth > 0 ? (hostWidth - centerWidth) / 2 : hostWidth / 2;
        const centerLayout = assignRow("center", centerX, centerStates);

        const state = {
            "left": {"groupX": ArchipelagoConfig.edgeMargin, "groupWidth": 0, "assignments": []},
            "center": centerLayout,
            "right": {"groupX": hostWidth - ArchipelagoConfig.edgeMargin, "groupWidth": 0, "assignments": []},
            "byModule": ({})
        };
        mergeAssignments(state, centerLayout);

        const leftLayoutItems = collectLayoutItems(leftItems);
        const rightLayoutItems = collectLayoutItems(rightItems);
        if (centerLayout.groupWidth > 0) {
            const leftBudget = Math.max(0, centerLayout.groupX - ArchipelagoConfig.edgeMargin - ArchipelagoConfig.islandGap);
            const centerEnd = centerLayout.groupX + centerLayout.groupWidth;
            const rightBudget = Math.max(0, hostWidth - ArchipelagoConfig.edgeMargin - centerEnd - ArchipelagoConfig.islandGap);
            const leftStates = scheduleRowStates(leftLayoutItems, leftBudget);
            const rightStates = scheduleRowStates(rightLayoutItems, rightBudget);
            state.left = assignRow("left", ArchipelagoConfig.edgeMargin, leftStates);
            state.right = assignRow("right",
                                    hostWidth - ArchipelagoConfig.edgeMargin - computeRowWidth(rightStates),
                                    rightStates);
        } else {
            const sideLayouts = scheduleSideRows(leftLayoutItems, rightLayoutItems, budget);
            state.left = sideLayouts.left;
            state.right = sideLayouts.right;
        }

        mergeAssignments(state, state.left);
        mergeAssignments(state, state.right);
        return state;
    }
}
