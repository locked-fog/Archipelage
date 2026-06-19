# Archipelago Notification Center

Third-party Archipelago plugin that owns `org.freedesktop.Notifications` and presents notifications through the island, preview, and expanded center surfaces.

## Build

```bash
cd plugins/notification-center
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

If another notification daemon already owns `org.freedesktop.Notifications`, the plugin stays loaded but reports the D-Bus ownership error in the expanded panel.

## Install Layout

- UI plugin: `/usr/share/archipelago/plugins/notifications/`
- Backend QML module: `/usr/share/archipelago/qml/ArchipelagoPlugins/Notifications/`

## Package

Create the source archive expected by `PKGBUILD` from the repository root:

```bash
tar --exclude='./plugins/notification-center/build' \
  --exclude='./plugins/notification-center/pkg' \
  --exclude='./plugins/notification-center/archipelago-plugin-notification-center-*' \
  -czf plugins/notification-center/archipelago-plugin-notification-center-0.1.0.tar.gz \
  --transform='s#^plugins/notification-center#archipelago-plugin-notification-center-0.1.0#' \
  plugins/notification-center
```

Then build and publish:

```bash
cd plugins/notification-center
makepkg -f --cleanbuild
install -Dm644 archipelago-plugin-notification-center-0.1.0-5-x86_64.pkg.tar.zst /srv/pacman/localrepo/x86_64/
repo-add /srv/pacman/localrepo/x86_64/localrepo.db.tar.gz /srv/pacman/localrepo/x86_64/archipelago-plugin-notification-center-0.1.0-5-x86_64.pkg.tar.zst
```
