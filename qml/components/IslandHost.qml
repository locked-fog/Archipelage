import QtQuick
import ArchipelagoBackend

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
    readonly property real leftX: leftRow.x
    readonly property real leftWidth: leftRow.visible ? leftRow.width : 0
    readonly property real centerX: centerRow.x
    readonly property real centerWidth: centerRow.visible ? centerRow.width : 0
    readonly property real rightX: rightRow.x
    readonly property real rightWidth: rightRow.visible ? rightRow.width : 0
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
        const entries = parent && parent.moduleRegistry ? parent.moduleRegistry : {};
        const manifestDefaults = [];
        const ids = Object.keys(entries);
        for (let index = 0; index < ids.length; index++) {
            const id = ids[index];
            const entry = entries[id] || {};
            const anchors = entry.anchors || [];
            if (configuredIds[id] || anchors.indexOf(anchorName) < 0)
                continue;
            manifestDefaults.push({
                id: id,
                priority: Number(entry.defaultPriority || 0)
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
        const entry = parent && parent.moduleEntry ? parent.moduleEntry(moduleId) : {};
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
        return parent && parent.compactComponentFor ? parent.compactComponentFor(moduleId) : null;
    }

    function expandedFor(moduleId) {
        return parent && parent.expandedComponentFor ? parent.expandedComponentFor(moduleId) : null;
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
        return rowItems(leftRow).concat(rowItems(centerRow)).concat(rowItems(rightRow));
    }

    function rowItems(row) {
        const result = [];
        const children = row.children;
        for (let index = 0; index < children.length; index++) {
            const child = children[index];
            if (child && child.moduleId !== undefined)
                result.push(child);
        }
        return result;
    }

    function moduleItem(moduleId) {
        const items = moduleItems();
        for (let index = 0; index < items.length; index++) {
            const item = items[index];
            if (item && item.moduleId === moduleId && item.visible)
                return item;
        }
        return null;
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
        const rows = [leftRow, centerRow, rightRow];
        for (let rowIndex = 0; rowIndex < rows.length; rowIndex++) {
            const children = rows[rowIndex].children;
            for (let childIndex = 0; childIndex < children.length; childIndex++) {
                const child = children[childIndex];
                if (child && child.moduleId === moduleId && child.visible) {
                    const point = child.mapToItem(root.parent, 0, 0);
                    return {
                        "x": point.x,
                        "y": point.y,
                        "width": child.width,
                        "height": child.height
                    };
                }
            }
        }
        return null;
    }

    Row {
        id: leftRow

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: ArchipelagoConfig.edgeMargin
        anchors.topMargin: root.topMargin
        spacing: ArchipelagoConfig.islandGap

        Repeater {
            model: root.modulesForAnchor("left")

            IslandModule {
                required property string modelData

                moduleId: modelData
                host: root
                compactLevel: root.compactLevel
                visible: root.shouldShowModule(modelData)
            }
        }
    }

    Row {
        id: centerRow

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: root.topMargin
        spacing: ArchipelagoConfig.islandGap

        Repeater {
            model: root.modulesForAnchor("center")

            IslandModule {
                required property string modelData

                moduleId: modelData
                host: root
                compactLevel: root.compactLevel
                visible: root.shouldShowModule(modelData)
            }
        }
    }

    Row {
        id: rightRow

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: ArchipelagoConfig.edgeMargin
        anchors.topMargin: root.topMargin
        spacing: ArchipelagoConfig.islandGap

        Repeater {
            model: root.modulesForAnchor("right")

            IslandModule {
                required property string modelData

                moduleId: modelData
                host: root
                compactLevel: root.compactLevel
                visible: root.shouldShowModule(modelData)
            }
        }
    }
}
