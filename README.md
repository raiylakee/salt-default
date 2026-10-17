# Salt-Default

Windows-style **Default Apps** settings panel for Linux, built with **Meson + C++26 + gtkmm-4.0**.

Linux equivalent of Windows *Settings → Apps → Default apps*, backed by the XDG MIME database (`mimeapps.list` + `.desktop` files via `Gio::AppInfo`).

## Windows feature mapping

| Windows Default Apps | Linux port in this project |
|---|---|
| Search apps | Per-page `Gtk::SearchEntry` (Apps / File types / Link types) |
| App list → app details | Left `Gtk::ListBox` of `.desktop` apps → right detail pane |
| “Set default” button | Sets app as default for **all** its `MimeType=` entries via `set_as_default_for_type()` → `~/.config/mimeapps.list` |
| Per-type rows with current default | Each MIME row shows `get_default_for_type()` name + **Change** button |
| Defaults by file type | “File types” page: all registered MIME types, extensions from `/usr/share/mime/globs` |
| Defaults by link type | “Link types” page: `x-scheme-handler/http, https, mailto, …` |
| Chooser dialog | `Gtk::Dialog` listing `get_all_for_type()` candidates, current marked `● Default` |

## Project layout

```
meson.build          # project(salt_default, cpp, gtkmm-4.0)
src/meson.build
src/main.cpp         # Gtk::Application bootstrap
src/app_window.h/.cpp# MainWindow: HeaderBar + StackSidebar + Stack (Apps/File types/Link types)
src/mime_service.h/.cpp # MimeService: Gio::AppInfo backend, globs parsing
src/log.h            # header-only spdlog-compatible logger (avoids hard spdlog dep)
```

Previous prototype only showed a dummy `Stack` with “Page 1/2” and printed
`mimeapps.list` lines to stdout. It is now replaced by the service + window above.

## Build & run

Requires: `meson`, `ninja`, `g++` (C++26), `gtkmm-4.0`, `clang-format` (optional).

```sh
just setup   # meson setup builddir
just build   # meson compile -C builddir
just run     # ./builddir/salt_default
just format  # clang-format -i src/*
```

Or manually:

```sh
meson setup builddir
meson compile -C builddir
./builddir/salt_default
```

No `spdlog` package needed — `src/log.h` provides `spdlog::info/debug/warn/error`
on top of `<print>`.

## Notes

- Reads: `Gio::AppInfo::get_all()`, `content_types_get_registered()`,
  XDG globs (`$XDG_DATA_HOME` + `$XDG_DATA_DIRS` + `/usr/share/mime/globs`),
  `~/.config/mimeapps.list` (via GIO).
- Writes: `Gio::AppInfo::set_as_default_for_type()` → `[Default Applications]`
  in `~/.config/mimeapps.list`.
- XDG correctness: `Hidden=true` skipped, `TryExec` missing-binary skipped
  (Desktop Entry spec), `should_show()` for NoDisplay/OnlyShowIn/NotShowIn.
  Type rows show `• user` vs `• system` provenance; chooser offers
  “Reset to system” (`reset_type_associations`). `GFileMonitor` on
  `mimeapps.list` live-reloads external changes (`gio mime`, portals).
- File-type list is capped at 800 rows with no query to keep the UI fast;
  search to narrow down.
