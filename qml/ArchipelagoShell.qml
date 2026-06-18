import QtQuick
import Quickshell
import Quickshell.Wayland
import ArchipelagoCore
import "components"
import "services"

PanelWindow {
    id: root

    property var shellRootController: null
    readonly property bool screenFocused: true
    readonly property int compactTop: 4
    readonly property int compactBottom: compactTop + ArchipelagoConfig.islandHeight
    readonly property bool expandedOpen: islandHost.expandedMounted
        || previewController.mounted

    // The plugin registry. Populated by the IslandRegistry child below
    // at Component.onCompleted. IslandHost and per-module expanded
    // surfaces both read from moduleRegistry instead of branching on
    // moduleId themselves.
    // The only built-in plugin is qml/plugins/time. Third-party UI
    // plugins can be installed under
    // ~/.local/share/archipelago/plugins/<id>/ or
    // /usr/share/archipelago/plugins/<id>/.
    // preferredWidth / preferredHeight of 0 fall back to
    // ArchipelagoConfig.expandedWidth / expandedHeight.
    property var moduleRegistry: registry.entries

    function moduleEntry(moduleId) {
        return moduleRegistry && moduleId ? (moduleRegistry[moduleId] || {}) : {};
    }

    function compactComponentFor(moduleId) {
        return moduleEntry(moduleId).compact || null;
    }

    function expandedComponentFor(moduleId) {
        return moduleEntry(moduleId).expanded || null;
    }

    function previewTemplateFor(moduleId, templateId) {
        const templates = moduleEntry(moduleId).previewTemplates || {};
        return templates[templateId] || null;
    }

    function showPreview(moduleId, templateId, payload, options) {
        if (!moduleId || !templateId)
            return "";

        const opts = {};
        const sourceOptions = options || {};
        for (const key in sourceOptions)
            opts[key] = sourceOptions[key];
        if (!opts.originRect)
            opts.originRect = originRectForModule(moduleId);
        return previewController.show(moduleId, templateId, payload || {}, opts);
    }

    function dismissPreview(instanceId) {
        previewController.dismiss(instanceId || "");
    }

    function dismissPreviewLayout(moduleId, templateId) {
        previewController.dismissLayout(moduleId || "", templateId || "");
    }

    function dismissAllPreviews() {
        previewController.dismissAll();
    }

    function preferredExpandedWidth(moduleId) {
        const preferred = moduleEntry(moduleId).preferredWidth;
        return preferred && preferred > 0 ? preferred : ArchipelagoConfig.expandedWidth;
    }

    function preferredExpandedHeight(moduleId) {
        const preferred = moduleEntry(moduleId).preferredHeight;
        return preferred && preferred > 0 ? preferred : ArchipelagoConfig.expandedHeight;
    }

    function styleColor(tokenName, fallback) {
        const override = ArchipelagoConfig.styleOverride(tokenName);
        return override !== "" ? override : fallback;
    }

    function openExpanded(moduleId, originRect) {
        if (!moduleId)
            return;
        islandHost.openExpandedModule(moduleId, originRect);
    }

    function closeExpanded() {
        dismissAllPreviews();
        islandHost.closeExpanded();
    }

    function toggleModule(moduleId) {
        if (islandHost.expandedModule === moduleId) {
            closeExpanded();
            return;
        }
        islandHost.activateModuleById(moduleId);
    }

    function originRectForModule(moduleId) {
        const rect = islandHost.originRectForModule(moduleId);
        if (rect !== null)
            return rect;
        return {
            "x": Math.max(ArchipelagoConfig.edgeMargin, root.width - ArchipelagoConfig.edgeMargin - ArchipelagoConfig.islandHeight),
            "y": root.compactTop,
            "width": ArchipelagoConfig.islandHeight,
            "height": ArchipelagoConfig.islandHeight
        };
    }

    function originRectForItem(item) {
        if (!item)
            return null;
        const point = item.mapToItem(previewController, 0, 0);
        return {
            "x": point.x,
            "y": point.y,
            "width": item.width,
            "height": item.height
        };
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
            x: Math.floor(islandHost.expandedMaskX)
            y: Math.floor(islandHost.expandedMaskY)
            width: Math.ceil(islandHost.expandedMaskWidth)
            height: Math.ceil(islandHost.expandedMaskHeight)
        }

        Region {
            intersection: Intersection.Combine
            x: Math.floor(previewController.maskX)
            y: Math.floor(previewController.maskY)
            width: Math.ceil(previewController.maskWidth)
            height: Math.ceil(previewController.maskHeight)
        }
    }

    implicitHeight: Math.max(
        islandHost.expandedMounted ? Math.ceil(islandHost.expandedMaskY + islandHost.expandedMaskHeight + 12) : 0,
        previewController.mounted ? Math.ceil(previewController.maskY + previewController.maskHeight + 12) : 0,
        Math.ceil(ArchipelagoConfig.islandHeight + 8)
    )
    exclusiveZone: Math.ceil(ArchipelagoConfig.islandHeight + 7)
    aboveWindows: true
    focusable: islandHost.expandedOpened || previewController.hasFocusedPreview
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.keyboardFocus: (islandHost.expandedOpened || previewController.hasFocusedPreview)
        ? WlrKeyboardFocus.OnDemand
        : WlrKeyboardFocus.None

    MouseArea {
        anchors.fill: parent
        enabled: islandHost.expandedMounted || previewController.mounted
        acceptedButtons: Qt.LeftButton
        onClicked: {
            if (previewController.mounted)
                root.dismissAllPreviews()
            else
                root.closeExpanded()
        }
    }

    IslandHost {
        id: islandHost

        anchors.fill: parent
        topMargin: root.compactTop
        shellWindow: root

        onModuleTriggered: function(moduleId, originRect) {
            root.openExpanded(moduleId, originRect);
        }
    }

    PreviewController {
        id: previewController

        anchors.fill: parent
        shellWindow: root
    }

    // Plugin registry. Discovers the built-in Time example plus
    // user/system third-party plugin roots and exposes a uniform entries
    // map. moduleRegistry above reads from registry.entries, so all
    // framework dispatch flows through here.
    IslandRegistry {
        id: registry
    }
}
