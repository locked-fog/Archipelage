# Archipelago Media Control Center

Third-party Archipelago plugin that exposes an MPRIS media island.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Package

Create the source archive expected by `PKGBUILD` from the repository root:

```bash
tar --exclude='./plugins/media-control-center/build' \
  --exclude='./plugins/media-control-center/pkg' \
  --exclude='./plugins/media-control-center/archipelago-plugin-media-control-center-*' \
  -czf plugins/media-control-center/archipelago-plugin-media-control-center-0.1.0.tar.gz \
  --transform='s#^plugins/media-control-center#archipelago-plugin-media-control-center-0.1.0#' \
  plugins/media-control-center
```

Then build and publish:

```bash
cd plugins/media-control-center
makepkg -f --cleanbuild
install -Dm644 archipelago-plugin-media-control-center-0.1.0-1-x86_64.pkg.tar.zst /srv/pacman/localrepo/x86_64/
repo-add /srv/pacman/localrepo/x86_64/localrepo.db.tar.gz /srv/pacman/localrepo/x86_64/archipelago-plugin-media-control-center-0.1.0-1-x86_64.pkg.tar.zst
```
