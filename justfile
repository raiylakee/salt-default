builddir := "builddir"
exe := builddir + "/salt_default"
prefix := "/usr/local"

default:
    @just --list

setup:
    meson setup {{builddir}} --prefix {{prefix}}

wipe:
    meson setup {{builddir}} --wipe --prefix {{prefix}}

build:
    meson compile -C {{builddir}}

run: build
    ./{{exe}}

install: build
    meson install -C {{builddir}}

# Install into a staging dir (for packaging / testing without touching the system)
stage: build
    DESTDIR="{{builddir}}/stage" meson install -C {{builddir}} --no-rebuild
    @echo "Staged under {{builddir}}/stage"

uninstall:
    ninja -C {{builddir}} uninstall

format:
    find src -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | xargs clang-format -i

lint:
    find src -name '*.cpp' -o -name '*.h' | xargs clang-tidy -p {{builddir}}

clean:
    rm -rf {{builddir}}

# --- packaging ---------------------------------------------------------------

# Arch Linux (needs base-devel); builds and installs the local package.
aur:
    makepkg -si

# Debian / Ubuntu: writes ../salt-default_<version>_<arch>.deb
deb:
    dpkg-buildpackage -b -us -uc

# Fedora / RHEL / openSUSE
rpm:
    rpmbuild -ba rpm/salt-default.spec

# Any distribution (bundles gtkmm; needs org.gnome.Sdk//51 installed)
flatpak:
    flatpak-builder --user --install --force-clean {{builddir}}-flatpak flatpak/org.salt.default.yml

# Metadata sanity checks used by the packages above.
check-metadata:
    desktop-file-validate data/salt-default.desktop
    appstreamcli validate --no-net data/org.salt.default.metainfo.xml

