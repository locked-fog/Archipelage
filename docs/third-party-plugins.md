# Third-Party Plugins

Archipelago discovers and loads UI plugins. The core package ships only the `Time` plugin as code documentation. Plugins can import `ArchipelagoCore 1.0` for shell-owned configuration, style tokens, and plugin discovery. A third-party plugin that needs backend behavior should ship its own Qt QML module and import it from the plugin QML.

Archipelago does not provide a backend plugin ABI, manifest-driven backend startup, dependency resolution, permission management, dynamic C++ module unload, or process isolation in this version.

## Core API

`ArchipelagoCore 1.0` is intentionally small. It contains:

- `ArchipelagoConfig` for the user config and shell sizing/style preferences.
- `StyleTokens` for the shared visual language.
- `PluginScanner` for shell-owned plugin discovery.

System features such as battery, brightness, notifications, media, and workspace integrations should live in plugin-owned QML modules rather than the core API.

## UI Plugin Roots

UI plugins use the same structure as the built-in `Time` example:

```text
<plugin-root>/<plugin-id>/
  manifest.json
  Compact.qml
  Expanded.qml
```

`manifest.json` may also declare optional compact layout defaults:

```json
{
  "compactLayout": {
    "preferredWidth": 180,
    "minimumWidth": 44,
    "maximumWidth": 320,
    "visible": true,
    "priority": 70
  }
}
```

`Compact.qml` may expose runtime layout requests:

```qml
property int preferredCompactWidth
property int minimumCompactWidth
property int maximumCompactWidth
property bool compactVisibleRequested
property int compactLayoutPriority
```

The shell treats these as requests. User config and host space constraints stay authoritative. `modules.<id>.width` now acts as a preferred compact width, not a hard final width.

Archipelago scans these roots in order:

1. `$ARCHIPELAGO_PLUGINS_DIR` if set. This overrides the user/system third-party roots below.
2. `$XDG_DATA_HOME/archipelago/plugins`, or `~/.local/share/archipelago/plugins`.
3. `$XDG_DATA_DIRS/archipelago/plugins`, defaulting to `/usr/share/archipelago/plugins` and `/usr/local/share/archipelago/plugins`.
4. The development and installed built-in roots under `qml/plugins`, currently only the `Time` example. These roots are still scanned when `$ARCHIPELAGO_PLUGINS_DIR` is set, so the built-in `Time` plugin remains available unless an earlier root provides the same manifest `id`.

When multiple roots provide the same manifest `id`, the first one wins.

## Backend Modules

Install backend QML modules into an import root that the launcher prepends to `QML2_IMPORT_PATH`:

```text
~/.local/share/archipelago/qml/ArchipelagoPlugins/Example/
  qmldir
  libexampleplugin.so
  ArchipelagoPluginsExample.qmltypes
```

System installs can use:

```text
/usr/share/archipelago/qml/ArchipelagoPlugins/Example/
/usr/local/share/archipelago/qml/ArchipelagoPlugins/Example/
```

Then import the module from plugin UI:

```qml
import QtQuick
import ArchipelagoPlugins.Example 1.0

Item {
    Component.onCompleted: ExampleService.registerClient("compact")
    Component.onDestruction: ExampleService.releaseClient("compact")
}
```

`manifest.json` can still declare `dataNeeds`, but that field is informational. Archipelago does not start or authorize backend modules from it.

## Lifecycle Convention

Backend singletons should expose idempotent methods such as `start()`, `stop()`, `refresh()`, and domain-specific actions. UI components should register on `Component.onCompleted` and release on `Component.onDestruction`.

The singleton should keep an internal client reference count. When the last client is released, stop polling, cancel outstanding requests, and release temporary resources. If the singleton launched a helper process, terminate that helper during shutdown as a fallback. If it talks to an external system service, only stop the plugin-owned requests and polling.

## Pacman Removal Cleanup

The Arch package installs an ALPM hook for removing Archipelago. The hook runs before package removal and deletes unowned plugin directories under:

```text
/usr/share/archipelago/plugins
/usr/share/archipelago/qml/ArchipelagoPlugins
```

Paths still owned by another pacman package are preserved. User plugins under `~/.local/share/archipelago` are never removed by the package hook.
