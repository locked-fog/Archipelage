# Archipelago

Archipelago is a niri-first Quickshell system shell built from multiple floating islands instead of a single solid bar. It keeps the dark capsule visual language from Tide Island, but splits shell responsibilities into independently arranged modules.

## Status

Version: `1.0.0-r1`

This release is an MVP for niri:

- One built-in `Time` plugin as code documentation for the plugin shape.
- User, system, and package-installed third-party UI plugin roots.
- Third-party backend modules through normal Qt QML imports.
- One expanded surface at a time, morphing down from the triggering island.
- Preview surfaces for transient plugin-owned UI.
- niri workspace/window data through IPC, with reconnect and action failure reporting.
- Hot-reloaded JSON config at `~/.config/archipelago/config.json`.
- Backend service singletons that external plugins can consume.

Hyprland support is intentionally limited to a future adapter boundary.

## Dependencies

Runtime:

- `quickshell`
- `qt6-base`
- `qt6-declarative`
- `systemd-libs`
- `wireplumber`
- `libpulse`
- `brightnessctl`
- `upower`
- `pipewire`
- `niri`

Optional:

- `cava` for the media visualizer backend
- `tlp` and `polkit` for power profile changes

Build:

- `cmake`
- `ninja`
- `git`
- `qt6-base`
- `qt6-declarative`

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Manual Preview

From the project root:

```bash
QML2_IMPORT_PATH="$PWD/build:${QML2_IMPORT_PATH}" \
LD_LIBRARY_PATH="$PWD/build:$PWD/build/ArchipelagoBackend:${LD_LIBRARY_PATH}" \
quickshell -v -p "$PWD"
```

Stop it with `Ctrl+C` in the same terminal.

## Configuration

The user config path is:

```text
~/.config/archipelago/config.json
```

Start from the example config:

```bash
mkdir -p ~/.config/archipelago
cp examples/config.json ~/.config/archipelago/config.json
```

The config supports:

- Module enable/disable.
- Island anchors and ordering.
- Module priority and compact display mode.
- Font families and font sizes.
- Island size, gap, expanded panel size, and animation timing.
- Style token color overrides.

Invalid JSON or out-of-range values fall back to defaults and are surfaced in the shell UI.

## Preview Surfaces

Plugins can register transient preview layouts in `manifest.json` under `previewTemplates`.
Runtime code fills those layouts with payload data through the shell preview controller.
The core package does not ship notification, connectivity, media, or workspace
preview templates; those should come from external plugins.

## Third-Party Plugins

UI plugins can be installed under `~/.local/share/archipelago/plugins/<plugin-id>/`
or `/usr/share/archipelago/plugins/<plugin-id>/` using the same
`manifest.json`, `Compact.qml`, and `Expanded.qml` shape as the built-in
`Time` plugin.

Plugins that need backend behavior should ship an independent Qt QML module,
for example `import ArchipelagoPlugins.Example 1.0`, installed under
`~/.local/share/archipelago/qml` or the matching system QML roots. The launcher
prepends those roots to `QML2_IMPORT_PATH` and preserves any existing user value.

See [docs/third-party-plugins.md](docs/third-party-plugins.md) for the full
directory layout and lifecycle convention.

For pacman installs, Archipelago also installs an ALPM remove hook. On package
removal it deletes unowned system plugin directories under `/usr/share` while
leaving files owned by other pacman packages and all user plugins untouched.

## Install

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
DESTDIR="$PWD/pkgroot" cmake --install build
```

The installed launcher is:

```bash
archipelago
```

Systemd user service:

```bash
systemctl --user enable --now archipelago.service
```

## Arch Package

Build the package from the current git tag:

```bash
git archive --format=tar.gz --prefix=archipelago-1.0.0/ \
  -o archipelago-1.0.0.tar.gz v1.0.0-r1
makepkg -f --cleanbuild
```

Publish to a local pacman repository:

```bash
install -Dm644 archipelago-1.0.0-1-x86_64.pkg.tar.zst /srv/pacman/localrepo/x86_64/
repo-add /srv/pacman/localrepo/x86_64/localrepo.db.tar.gz /srv/pacman/localrepo/x86_64/archipelago-1.0.0-1-x86_64.pkg.tar.zst
```

## License

MIT. See [LICENSE](LICENSE).
