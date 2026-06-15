import QtQuick
import Quickshell
import ArchipelagoBackend

// IslandRegistry exposes a uniform entry map for the framework.
// The actual disk scan is done by the C++ PluginScanner singleton;
// IslandRegistry just builds the { id → Component } map from the
// manifest list and watches for rescan notifications.
//
// Adding a new built-in module means:
//   1. drop a directory under qml/plugins/<id>/
//   2. (optional) write qml/plugins/<id>/manifest.json
//   3. (optional) write qml/plugins/<id>/Compact.qml
//   4. (optional) write qml/plugins/<id>/Expanded.qml
//
// No edit to IslandRegistry.qml or ArchipelagoShell.qml is required.
// Both are now data-driven from PluginScanner.
Item {
    id: root

    // pluginsBase is exposed for diagnostics; PluginScanner already
    // resolves it from $ARCHIPELAGO_PLUGINS_DIR / $PWD/qml/plugins /
    // <install>/share/archipelago/qml/plugins. We mirror it here so
    // QML can build absolute file URLs from the same root.
    readonly property url pluginsBase: PluginScanner.pluginsBase !== ""
        ? Qt.url("file://" + PluginScanner.pluginsBase)
        : Qt.resolvedUrl("../plugins")

    // Public API consumed by ArchipelagoShell:
    //   entries[id] = { id, compact, expanded, preferredWidth, preferredHeight }
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
        const dirUrl = pluginsBase + "/" + id + "/";
        const compact = manifest.compact ? tryLoad(dirUrl + manifest.compact) : { component: null, error: null };
        const expanded = manifest.expanded ? tryLoad(dirUrl + manifest.expanded) : { component: null, error: null };

        if (compact.error)
            console.warn("[IslandRegistry] compact load failed for", id, ":", compact.error);
        if (expanded.error)
            console.warn("[IslandRegistry] expanded load failed for", id, ":", expanded.error);

        return {
            id: id,
            compact: compact.component,
            expanded: expanded.component,
            preferredWidth: Number(manifest.preferredWidth || 0),
            preferredHeight: Number(manifest.preferredHeight || 0)
        };
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
