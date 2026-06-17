# Maintainer: Locked_Fog <aboxyxy@outlook.com>

pkgname=archipelago
pkgver=1.1.0
pkgrel=1
pkgdesc="niri-first multi-island Quickshell system shell"
arch=('x86_64')
url="https://github.com/Locked-Fog/Archipelago"
license=('MIT')
depends=(
  'brightnessctl'
  'libpulse'
  'niri'
  'pipewire'
  'qt6-base'
  'qt6-declarative'
  'quickshell'
  'systemd-libs'
  'upower'
  'wireplumber'
)
makedepends=(
  'cmake'
  'git'
  'ninja'
)
checkdepends=(
  'qt6-base'
)
optdepends=(
  'cava: media visualizer support'
  'tlp: power profile control'
  'polkit: privileged power profile prompts through pkexec'
)
options=('!debug')
source=("${pkgname}-${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
  cmake -S "$srcdir/$pkgname-$pkgver" -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build build
}

check() {
  ctest --test-dir build --output-on-failure
}

package() {
  DESTDIR="$pkgdir" cmake --install build
}
