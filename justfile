builddir := "builddir"
exe := builddir + "/salt_default"
prefix := "/usr/local"
sources := "find src tests -type f"

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

test: build
    meson test -C {{builddir}} --print-errorlogs

install: build
    meson install -C {{builddir}}

# Install into a staging dir (for packaging / testing without touching the system)
stage: build
    DESTDIR="{{builddir}}/stage" meson install -C {{builddir}} --no-rebuild
    @echo "Staged under {{builddir}}/stage"

uninstall:
    ninja -C {{builddir}} uninstall

format:
    {{sources}} \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 | xargs -0 clang-format -i

check-format:
    {{sources}} \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 | xargs -0 clang-format --dry-run --Werror

lint: build
    {{sources}} \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-tidy -p {{builddir}}

clean:
    rm -rf {{builddir}}

# --- packaging ---------------------------------------------------------------

# Arch Linux (needs base-devel); builds and installs the local package.
aur:
    makepkg -si

# Refresh the source sums and regenerate .SRCINFO. Needs the release tag to
# exist (CHANGELOG: v0.0.1) because makepkg has to fetch the tarball.
aur-sums:
    updpkgsums
    makepkg --printsrcinfo > .SRCINFO

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
    desktop-file-validate data/org.salt.default.desktop
    appstreamcli validate --no-net data/org.salt.default.metainfo.xml
    sh -n flatpak/salt-default-wrapper.sh
