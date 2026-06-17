import QtQuick
import Quickshell
import ArchipelagoCore

// IslandRegistry exposes a uniform entry map for the framework.
// The actual disk scan is done by the ArchipelagoCore PluginScanner singleton;
// IslandRegistry just builds the { id → Component } map from the
// manifest list and watches for rescan notifications.
//
// The core package keeps qml/plugins/time as the code-as-documentation
// example. Third-party UI modules should be installed under:
//   ~/.local/share/archipelago/plugins/<id>/
//   /usr/share/archipelago/plugins/<id>/
//
// Plugin directories use the same manifest.json, Compact.qml, and
// Expanded.qml shape as the time example.
//
// No edit to IslandRegistry.qml or ArchipelagoShell.qml is required.
// Both are now data-driven from PluginScanner.
Item {
    id: root

    // pluginsBase is exposed for diagnostics. Each manifest also
    // carries directoryPath so entries from multiple roots can be
    // loaded side by side.
    readonly property url pluginsBase: PluginScanner.pluginsBase !== ""
        ? Qt.url("file://" + PluginScanner.pluginsBase)
        : Qt.resolvedUrl("../plugins")

    // Public API consumed by ArchipelagoShell:
    //   entries[id] = {
    //       id, compact, expanded, preferredWidth, preferredHeight,
    //       compactLayout,
    //       previewTemplates: { templateId -> metadata + Component }
    //   }
    // compact / expanded are Component references (or null if missing
    // or empty in manifest). preferredWidth / preferredHeight of 0 fall
    // back to ArchipelagoConfig.
    property var entries: ({})

    // React to the scanner finishing a rescan (initial scan, manual
    // rescan, or runtime env-var change).
    Connections {
        target: PluginScanner
        function onPluginsChanged() {
            root.reload();
        }
    }

    Component.onCompleted: reload()

    // rebuild entries from the current PluginScanner.plugins list.
    function reload() {
        const next = {};
        const list = PluginScanner.plugins || [];
        for (let i = 0; i < list.length; i++) {
            const manifest = list[i];
            const id = manifest.id;
            if (!id)
                continue;
            next[id] = loadEntry(manifest);
        }
        entries = next;
    }

    function loadEntry(manifest) {
        const id = manifest.id;
        const directoryName = manifest.directoryName || id;
        const dirUrl = manifest.directoryPath
            ? Qt.url("file://" + manifest.directoryPath + "/")
            : pluginsBase + "/" + directoryName + "/";
        const compact = manifest.compact ? tryLoad(dirUrl + manifest.compact) : { component: null, error: null };
        const expanded = manifest.expanded ? tryLoad(dirUrl + manifest.expanded) : { component: null, error: null };
        const previewTemplates = loadPreviewTemplates(manifest.previewTemplates || [], dirUrl, id);

        if (compact.error)
            console.warn("[IslandRegistry] compact load failed for", id, ":", compact.error);
        if (expanded.error)
            console.warn("[IslandRegistry] expanded load failed for", id, ":", expanded.error);

        return {
            id: id,
            directoryName: directoryName,
            label: manifest.label || id,
            anchors: manifest.anchors || [],
            defaultPriority: Number(manifest.defaultPriority || 0),
            dataNeeds: manifest.dataNeeds || [],
            compact: compact.component,
            expanded: expanded.component,
            previewTemplates: previewTemplates,
            preferredWidth: Number(manifest.preferredWidth || 0),
            preferredHeight: Number(manifest.preferredHeight || 0),
            compactLayout: manifest.compactLayout || ({})
        };
    }

    function loadPreviewTemplates(templateList, dirUrl, pluginId) {
        const result = {};
        for (let index = 0; index < templateList.length; index++) {
            const template = templateList[index] || {};
            const templateId = template.id || "";
            const componentFile = template.component || "";
            if (templateId === "" || componentFile === "")
                continue;

            const loaded = tryLoad(dirUrl + componentFile);
            if (loaded.error) {
                console.warn("[IslandRegistry] preview load failed for", pluginId + "/" + templateId, ":", loaded.error);
                continue;
            }

            result[templateId] = {
                id: templateId,
                component: loaded.component,
                defaultWidth: Number(template.defaultWidth || 360),
                defaultHeight: Number(template.defaultHeight || 112),
                timeoutMs: Number(template.timeoutMs || 5000),
                maxVisible: Number(template.maxVisible || 5),
                focusPolicy: template.focusPolicy || "passive"
            };
        }
        return result;
    }

    function tryLoad(url) {
        const c = Qt.createComponent(url);
        if (!c)
            return { component: null, error: "Qt.createComponent returned null for " + url };
        if (c.status === Component.Error)
            return { component: null, error: c.errorString() };
        return { component: c, error: null };
    }
}
