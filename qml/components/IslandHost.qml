import QtQuick
import ArchipelagoBackend
import "../modules"

Item {
    id: root

    property string expandedModule: ""
    property int topMargin: 4
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

    signal moduleTriggered(string moduleId, var originRect)

    function modulesForAnchor(anchorName) {
        const list = anchorsConfig[anchorName];
        return list === undefined || list === null ? [] : list;
    }

    function moduleConfig(moduleId) {
        const config = ArchipelagoConfig.moduleConfig(moduleId);
        return config === undefined || config === null ? ({}) : config;
    }

    function modulePriority(moduleId) {
        const config = moduleConfig(moduleId);
        return Number(config.priority || 0);
    }

    function shouldShowModule(moduleId) {
        if (moduleId === "notifications")
            return false;
        if (!ArchipelagoConfig.moduleEnabled(moduleId))
            return false;
        if (compactLevel >= 2 && moduleId === "media")
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
        const rows = [leftRow, centerRow, rightRow];
        for (let rowIndex = 0; rowIndex < rows.length; rowIndex++) {
            const children = rows[rowIndex].children;
            for (let childIndex = 0; childIndex < children.length; childIndex++) {
                const child = children[childIndex];
                if (child && child.moduleId === moduleId && child.visible) {
                    activateModule(moduleId, child);
                    return;
                }
            }
        }
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
