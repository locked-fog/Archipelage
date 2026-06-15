import QtQuick
import Quickshell

// IslandRegistry discovers the available business modules ("plugins")
// and exposes a uniform entry map to the framework.
//
// In Phase 2a plugins are discovered by hard-coded id list (the
// five built-ins). In Phase 2b the list will be replaced by a C++
// helper that lists subdirectories of pluginsBase at startup, so
// that dropping a new directory under qml/plugins/<id>/ is enough
// to register a new business module — no edit to this file.
//
// Each entry exposes a Qt.createComponent result for Compact.qml and
// Expanded.qml, plus per-module expanded-surface sizing hints.
// ArchipelagoShell builds its moduleRegistry from this.entries.
Item {
    id: root

    // Phase 2a: built-in plugin ids. Phase 2b replaces this with
    // a directory scan; the rest of the file is already prepared.
    readonly property var _builtinIds: [
        "workspaces",
        "clock",
        "media",
        "system",
        "notifications"
    ]

    // Root of the plugins tree. Each subdirectory is one plugin,
    // named by the directory name. pluginsBase is the qml/ install
    // root by default; override for tests or custom layouts.
    property url pluginsBase: Qt.resolvedUrl("../plugins")

    // Public API consumed by ArchipelagoShell:
    //   entries[id] = { id, compact, expanded, preferredWidth, preferredHeight }
    // compact / expanded are Component references (or null if missing).
    // preferredWidth / preferredHeight of 0 fall back to ArchipelagoConfig.
    property var entries: ({})

    // reload() re-scans pluginsBase and rebuilds entries. Cheap to call
    // on demand (e.g. when the user adds a new plugin at runtime).
    function reload() {
        const next = {};
        for (const id of _builtinIds)
            next[id] = loadEntry(id);
        entries = next;
    }

    Component.onCompleted: reload()

    function loadEntry(id) {
        const base = pluginsBase + "/" + id + "/";
        const compact = tryLoad(base + "Compact.qml");
        const expanded = tryLoad(base + "Expanded.qml");

        if (compact.error)
            console.warn("[IslandRegistry] compact load failed for", id, ":", compact.error);
        if (expanded.error)
            console.warn("[IslandRegistry] expanded load failed for", id, ":", expanded.error);

        return {
            id: id,
            compact: compact.component,
            expanded: expanded.component,
            preferredWidth: 0,
            preferredHeight: 0
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
