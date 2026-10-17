# Salt-Default

Windows-style **Default Apps** settings panel for Linux, built with **Meson + C++26 + gtkmm-4.0**.

Linux equivalent of Windows *Settings → Apps → Default apps*, backed by the XDG MIME database (`mimeapps.list` + `.desktop` files via `Gio::AppInfo`).

## Windows feature mapping

| Windows Default Apps | Linux port in this project |
|---|---|
| Search apps | Per-page `Gtk::SearchEntry` (Apps / File types / Link types) |
| App list → app details | Left virtualized `Gtk::ListView` of `.desktop` apps → right detail pane |
| “Set default” button | Sets app as default for **all** its `MimeType=` entries via `set_as_default_for_type()` → `~/.config/mimeapps.list` |
| Per-type rows with current default | Each MIME row shows `get_default_for_type()` name + **Change** button |
| Defaults by file type | “File types” page: all registered MIME types, extensions from XDG globs |
| Defaults by link type | “Link types” page: `x-scheme-handler/http, https, mailto, …` |
| Chooser dialog | `Gtk::Dialog` listing `get_all_for_type()` candidates, current marked `● Default` |
| Remove / restore an app | `[Removed Associations]` management: remove an app, restore with **Allow** |

## Project layout

```
meson.build              # project(salt_default, cpp, gtkmm-4.0) + install rules
src/meson.build
src/main.cpp             # Gtk::Application bootstrap + zero-warning GDK defaults
src/app_id.h             # application id, shared with the desktop file name
src/app_window.h/.cpp    # MainWindow: HeaderBar + StackSidebar + Stack
src/rows.h/.cpp          # AppRow / TypeRow widgets used by the ListView factories
src/mime_service.h/.cpp  # MimeService: Gio::AppInfo + mimeapps.list backend
src/log.h                # header-only spdlog-compatible logger
tests/mime_service_test.cpp  # backend test, run by `just test`
data/org.salt.default.desktop       # application launcher
data/org.salt.default.metainfo.xml  # AppStream metadata
data/icons/hicolor/scalable/apps/org.salt.default.svg
PKGBUILD / .SRCINFO      # Arch Linux / AUR packaging
debian/                  # Debian / Ubuntu (.deb)
rpm/salt-default.spec    # Fedora / RHEL / openSUSE (.rpm)
flatpak/                 # distribution-independent Flatpak
.github/workflows/ci.yml # build, test, format, metadata and package checks
```

## Build & run

Requires: `meson`, `ninja`, `g++` (C++26), `gtkmm-4.0`, `clang-format` (optional).

```sh
just setup     # meson setup builddir
just build     # meson compile -C builddir
just run       # ./builddir/salt_default
just test      # meson test (backend test suite)
just format    # clang-format -i src/* tests/*
just check-format # read-only clang-format check, fails on drift
just lint      # clang-tidy, config in .clang-tidy
just install   # meson install (to /usr/local by default)
just stage     # install into builddir/stage, without touching the system
just uninstall # ninja uninstall
```

Or manually:

```sh
meson setup builddir --prefix /usr/local
meson compile -C builddir
./builddir/salt_default
```

No `spdlog` package needed — `src/log.h` provides `spdlog::info/debug/warn/error`
on top of `<print>`.

## Installing

```sh
meson setup builddir --prefix /usr/local
meson compile -C builddir
sudo meson install -C builddir
```

This installs the binary, the launcher (`org.salt.default.desktop`), the icon, and
the AppStream metadata. After installing you can remove the meson build dir with
`sudo ninja -C builddir uninstall`.

## Packaging

> **Verification status:** the plain meson build and the Flatpak are built,
> run and tested here. The `.deb`, `.rpm` and AUR paths are written but have
> not been built end to end on this machine — they need
> `dpkg-buildpackage` / `rpmbuild`, and the AUR source below needs the pushed
> `v0.0.1` tag to resolve. CI builds the `.deb` and checks that `.SRCINFO`
> matches `PKGBUILD`.

### Arch Linux / AUR

`PKGBUILD` and `.SRCINFO` are included. To build locally:

```sh
makepkg -si
```

To publish on the AUR:

```sh
# 1. tag a release so the source URL resolves, e.g. v0.0.1
# 2. create the AUR repo and push
git clone ssh://aur@aur.archlinux.org/salt-default.git
cp PKGBUILD .SRCINFO salt-default/
cd salt-default && git add -A && git commit -m "Initial upload" && git push
```

Once published, Arch users install with either helper:

```sh
yay -S salt-default
paru -S salt-default
```

> **Note:** the AUR only serves Arch-based distributions (Arch, Manjaro,
> EndeavourOS, CachyOS, …). `yay` / `paru` will not work on Debian, Ubuntu,
> Fedora or openSUSE — those need their own `.deb` / `.rpm` packages, which
> are provided below.

### Debian / Ubuntu (`.deb`)

A `debian/` directory is included, so the standard tooling works:

```sh
sudo apt install build-essential debhelper meson ninja-build libgtkmm-4.0-dev
dpkg-buildpackage -b -us -uc          # writes ../salt-default_0.0.1_amd64.deb
sudo apt install ../salt-default_0.0.1_amd64.deb
```

`debian/rules` uses the plain `dh` sequences — debhelper detects Meson on its
own. The package installs the binary, the launcher, the icon and the AppStream
metadata (`debian/install` lists them explicitly, and `dh_missing` is run with
`--fail-missing` so packaging can never silently drop a file).

### Fedora / RHEL / openSUSE (`.rpm`)

`rpm/salt-default.spec` builds from the tagged release tarball:

```sh
sudo dnf install rpm-build gcc-c++ meson ninja-build 'pkgconfig(gtkmm-4.0)' \
                 desktop-file-utils appstream
rpmbuild -ba rpm/salt-default.spec
```

On openSUSE replace the dependency names (`gtkmm4-devel` to build,
`libgtkmm-4_0-1` at runtime). `%check` runs `desktop-file-validate` and
`appstreamcli validate` against the installed files.

### Any distribution (Flatpak)

`flatpak/org.salt.default.yml` builds a Flatpak that bundles gtkmm (the GNOME
runtime ships GTK but not gtkmm):

```sh
flatpak install --user flathub org.gnome.Platform//51 org.gnome.Sdk//51
flatpak-builder --user --install --force-clean build-dir flatpak/org.salt.default.yml
flatpak run org.salt.default
```

> **Heads-up:** a default-applications panel is a host integration tool, so the
> sandbox is given `--filesystem=host-os` and `--filesystem=home`, and the
> entry point repoints `XDG_*` back at the host (plus `SALT_HOST_ROOT`, the
> sandbox's mount of the host root, so an absolute `Icon=` path still resolves).
> Those permissions are broader than Flathub allows, so this manifest is for
> self-distribution rather than a Flathub submission. See `flatpak/README.md`
> for the details.

## Notes

- Reads: `Gio::AppInfo::get_all()`, `content_types_get_registered()`,
  XDG globs (`$XDG_DATA_HOME` + `$XDG_DATA_DIRS`), and `mimeapps.list`.
- Writes: `Gio::AppInfo::set_as_default_for_type()` → `[Default Applications]`,
  plus direct `[Removed Associations]` management via `Glib::KeyFile`
  (comments and unrelated groups are preserved).
- XDG correctness: `Hidden=true` skipped, `TryExec` missing-binary skipped
  (Desktop Entry spec), `should_show()` for NoDisplay/OnlyShowIn/NotShowIn.
  Inside a Flatpak an unresolvable `Exec`/`TryExec` does **not** mean "cannot
  launch" (the host binaries simply are not in the sandbox), so there the
  desktop files GIO refuses are parsed directly and listed, the default shown
  for a type is resolved from `mimeinfo.cache` when GIO skipped the host
  candidate, and picking one of those writes `mimeapps.list` itself in the
  same shape GIO writes.
  Type rows show `• user` vs `• system` provenance and a `N removed` badge;
  the chooser offers “Reset to system” (`reset_type_associations`).
  A `GFileMonitor` on `mimeapps.list` live-reloads external changes
  (`gio mime`, portals) with a 400 ms debounce.
- One visibility rule is applied to both GIO's app infos and the directly
  parsed host desktop files (`Hidden`, `NoDisplay`, `OnlyShowIn`/`NotShowIn`,
  a missing `TryExec`, and the panel's own desktop file), and the type lists
  use the same rule. A type is therefore only listed when the application that
  claims it is listed too, and the three lists come out the same natively and
  inside the Flatpak: **37 apps, 1220 file types, 19 link types** in both.
  Two consequences are deliberate: a handler that exists only in the sandbox
  (the runtime's `org.gnome.Yelp.desktop`, for `help:`/`info:`/`man:`) is not
  offered, because the host cannot run it, and a link type whose only handler
  is a `NoDisplay` desktop file is not listed, because the panel would have no
  application to offer for it.
- The panel does not list itself: its own desktop file id is filtered out.
- `tests/mime_service_test.cpp` (`just test`) drives the whole backend against
  a fake XDG environment in a temporary directory, where GIO resolves nothing,
  so the sandbox fallback has to answer: fallback listing, icon resolution,
  the `mimeapps.list` writer, default resolution, and the visibility rules.
  `meson.build` sets `warning_level=3`; CI adds `-Dwerror=true`, checks
  `clang-format`, validates the desktop/AppStream metadata, verifies that
  `.SRCINFO` matches `PKGBUILD`, and builds the `.deb`.
- `mimeapps.list` is parsed once into a cache and invalidated on write /
  external change, instead of re-reading the file for every UI row.
- The three list pages use virtualized `Gtk::ListView`s over a
  `Gtk::StringList` model with a `Gtk::SignalListItemFactory`. Only the rows
  on screen get widgets (measured: 226 row widgets for the 821-entry file-type
  list, versus one widget tree per row with `Gtk::ListBox`), and `splice()`
  swaps the model contents on search without rebuilding widget trees.
  `src/rows.cpp` holds the `AppRow` / `TypeRow` widgets: the factory `setup`
  step builds the structure once, `bind` only fills in values.
- File-type list is capped at 800 rows with no query to keep the UI fast;
  search to narrow down.
- `main.cpp` sets `GSK_RENDERER=gl` and `GDK_DEBUG=no-portals` to keep the
  logs warning-free on hosts without a GPU or without xdg-desktop-portal.
  Both are skipped when `FLATPAK_ID` is set: inside a Flatpak the runtime
  picks the renderer, and `no-portals` would cut off the settings portal
  (host theme, fonts, colour scheme).
