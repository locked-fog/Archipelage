import QtQuick
import Quickshell
import Quickshell.Wayland
import ArchipelagoBackend
import "components"
import "modules"

PanelWindow {
    id: root

    property var shellRootController: null
    readonly property var niri: Compositor.niriService
    readonly property bool screenFocused: true
    readonly property int compactTop: 4
    readonly property int compactBottom: compactTop + ArchipelagoConfig.islandHeight
    readonly property bool expandedOpen: expandedSurface.opened || expandedSurface.mounted

    function styleColor(tokenName, fallback) {
        const override = ArchipelagoConfig.styleOverride(tokenName);
        return override !== "" ? override : fallback;
    }

    function openExpanded(moduleId, originRect) {
        if (!moduleId || moduleId === "notifications")
            return;
        islandHost.expandedModule = moduleId;
        expandedSurface.openModule(moduleId, originRect);
    }

    function closeExpanded() {
        expandedSurface.close();
    }

    function toggleModule(moduleId) {
        if (expandedSurface.opened && expandedSurface.moduleId === moduleId) {
            closeExpanded();
            return;
        }
        islandHost.activateModuleById(moduleId);
    }

    function showNotification(appName, summary, body) {
        if (expandedSurface.opened && expandedSurface.hovered)
            return;
        notificationCapsule.show(appName, summary, body);
    }

    color: StyleTokens.transparent
    anchors {
        top: true
        left: true
        right: true
    }

    mask: Region {
        Region {
            intersection: Intersection.Combine
            x: Math.floor(islandHost.leftX)
            y: Math.floor(islandHost.groupY)
            width: Math.ceil(islandHost.leftWidth)
            height: Math.ceil(islandHost.groupHeight)
        }

        Region {
            intersection: Intersection.Combine
            x: Math.floor(islandHost.centerX)
            y: Math.floor(islandHost.groupY)
            width: Math.ceil(islandHost.centerWidth)
            height: Math.ceil(islandHost.groupHeight)
        }

        Region {
            intersection: Intersection.Combine
            x: Math.floor(islandHost.rightX)
            y: Math.floor(islandHost.groupY)
            width: Math.ceil(islandHost.rightWidth)
            height: Math.ceil(islandHost.groupHeight)
        }

        Region {
            intersection: Intersection.Combine
            x: Math.floor(notificationCapsule.x)
            y: Math.floor(notificationCapsule.y)
            width: notificationCapsule.visible ? Math.ceil(notificationCapsule.width) : 0
            height: notificationCapsule.visible ? Math.ceil(notificationCapsule.height) : 0
        }

        Region {
            intersection: Intersection.Combine
            x: Math.floor(expandedSurface.maskX)
            y: Math.floor(expandedSurface.maskY)
            width: Math.ceil(expandedSurface.maskWidth)
            height: Math.ceil(expandedSurface.maskHeight)
        }
    }

    implicitHeight: Math.max(
        expandedOpen ? Math.ceil(expandedSurface.maskY + expandedSurface.maskHeight + 12) : 0,
        notificationCapsule.visible ? Math.ceil(notificationCapsule.y + notificationCapsule.height + 8) : 0,
        Math.ceil(ArchipelagoConfig.islandHeight + 8)
    )
    exclusiveZone: Math.ceil(ArchipelagoConfig.islandHeight + 7)
    aboveWindows: true
    focusable: expandedOpen
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.keyboardFocus: expandedOpen ? WlrKeyboardFocus.OnDemand : WlrKeyboardFocus.None

    IslandHost {
        id: islandHost

        anchors.fill: parent
        topMargin: root.compactTop

        onModuleTriggered: function(moduleId, originRect) {
            root.openExpanded(moduleId, originRect);
        }
    }

    NotificationCapsule {
        id: notificationCapsule

        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: root.compactTop + ArchipelagoConfig.islandHeight + 8
        width: Math.min(360, Math.max(260, parent.width - 2 * ArchipelagoConfig.edgeMargin))
        height: ArchipelagoConfig.islandHeight

        onOpenRequested: {
            const origin = {
                "x": notificationCapsule.x,
                "y": notificationCapsule.y,
                "width": notificationCapsule.width,
                "height": notificationCapsule.height
            };
            islandHost.expandedModule = "notifications";
            expandedSurface.openModule("notifications", origin);
        }
    }

    ExpandedSurface {
        id: expandedSurface

        shellWindow: root

        onClosed: {
            islandHost.expandedModule = "";
        }
    }

    Connections {
        target: root.niri

        function onActionFailed(actionName, message) {
            notificationCapsule.show("niri", actionName, message);
        }
    }
}
