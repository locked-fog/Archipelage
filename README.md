# Archipelago

Archipelago is a niri-first Quickshell system shell built from multiple floating islands instead of a single solid bar. It keeps the dark capsule visual language from Tide Island, but splits shell responsibilities into independently arranged modules.

## Status

Version: `1.0.0-r1`

This release is an MVP for niri:

- Compact islands for workspaces, clock, media, system status, and notifications.
- One expanded surface at a time, morphing down from the triggering island.
- niri workspace/window data through IPC, with reconnect and action failure reporting.
- Hot-reloaded JSON config at `~/.config/archipelago/config.json`.
- MPRIS media controls, battery, volume, brightness, screen-recording state, and notification capture.

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
