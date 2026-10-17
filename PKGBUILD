# Maintainer: Salt-Default Project <maintainer@example.com>
pkgname=salt-default
pkgver=0.0.1
pkgrel=1
pkgdesc='Windows-style Default Apps settings panel for Linux (GTK4 / XDG)'
arch=('x86_64' 'aarch64')
url='https://github.com/your-org/salt-default'
license=('GPL-3.0-or-later')
depends=('gtk4' 'gtkmm-4.0' 'glib2' 'glibmm-2.68')
makedepends=('meson' 'ninja' 'gcc' 'devtools')
provides=('salt-default')
conflicts=('salt-default-git')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  arch-meson "$pkgname-$pkgver" build
  meson compile -C build
}

package() {
  DESTDIR="$pkgdir" meson install -C build --no-rebuild
}
