# System Control Center

Third-party Archipelago plugin that exposes a compact system-status capsule and an expanded control center for connectivity, audio, brightness, and power state.

## Build

```bash
cd plugins/system-control-center
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Install Layout

- UI plugin: `/usr/share/archipelago/plugins/system-control/`
- Backend QML module: `/usr/share/archipelago/qml/ArchipelagoPlugins/SystemControl/`

## Package

Create the source archive expected by `PKGBUILD` from the repository root:

```bash
tar --exclude='./plugins/system-control-center/build' \
  --exclude='./plugins/system-control-center/pkg' \
  --exclude='./plugins/system-control-center/archipelago-plugin-system-control-center-*' \
  -czf plugins/system-control-center/archipelago-plugin-system-control-center-0.1.0.tar.gz \
  --transform='s#^plugins/system-control-center#archipelago-plugin-system-control-center-0.1.0#' \
  plugins/system-control-center
```

Then build and publish:

```bash
cd plugins/system-control-center
makepkg -f --cleanbuild
install -Dm644 archipelago-plugin-system-control-center-0.1.0-4-x86_64.pkg.tar.zst /srv/pacman/localrepo/x86_64/
repo-add /srv/pacman/localrepo/x86_64/localrepo.db.tar.gz /srv/pacman/localrepo/x86_64/archipelago-plugin-system-control-center-0.1.0-4-x86_64.pkg.tar.zst
```
