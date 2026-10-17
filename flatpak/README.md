# Flatpak packaging

`org.salt.default.yml` builds salt-default as a Flatpak. It bundles the whole
C++ binding stack (libsigc++, cairomm, glibmm, pangomm, gtkmm) because the
GNOME runtime ships GTK itself but **not** gtkmm.

The manifest pulls its source from GitHub, pinned to a commit (the comment in
the manifest shows the tagged form to use for a release and the `type: dir`
form to test a local checkout).

## Build and run (verified end to end)

```sh
# Runtimes. --user matters, see the gotchas below.
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.gnome.Platform//51 org.gnome.Sdk//51

# Build (run from the repository root)
flatpak-builder --user --force-clean --repo=repo builddir-flatpak flatpak/org.salt.default.yml

# Install the freshly built repo and run it
flatpak --user remote-add --no-gpg-verify --if-not-exists salt-local "$PWD/repo"
flatpak --user install -y salt-local org.salt.default
flatpak run org.salt.default
```

`--install` can replace the last three steps when you do not need a separate
repo to hand to other machines.

## Gotchas found while testing

| Symptom | Cause | Fix |
|---|---|---|
| `org.gnome.Sdk/x86_64/51 not installed` | `flatpak-builder` resolves runtimes from the **system** installation by default, but `--user` was used to install them | pass `--user` too |
| the deps step tries `flatpak --system install` and times out | `--install-deps-from=flathub` always installs **system-wide** | omit it when the runtimes are already installed |
| the build directory is copied into itself | the `type: dir` source copies the repository root and the build directory lives inside it | the commented `type: dir` variant in the manifest lists the build directory in `skip:` |
| `install: cannot stat 'salt-default-wrapper.sh'` | `post-install` does **not** run with the module source dir as its working directory | the manifest uses absolute `/run/build/salt-default/...` paths |
| `appstreamcli compose failed: missing-launchable-desktop-file` | Flatpak only exports `<app-id>.desktop` / `<app-id>.svg`, so the desktop file, the icon **and** the metainfo `<launchable>` must all agree | the source files are already named after the app id (`data/org.salt.default.desktop`, `…/org.salt.default.svg`, `<launchable>`), so nothing has to be rewritten |
| `flatpak-builder: command not found` | not packaged by every distribution | `flatpak run --command=flatpak-builder org.flatpak.Builder ...` (add `--disable-rofiles-fuse` if `/dev/fuse` is unavailable), or unpack the distribution package elsewhere and run its binary |

## Why this is not a Flathub app

A "Default Applications" panel is a **host integration** tool: it reads the
applications the host has installed and edits the host's
`~/.config/mimeapps.list`. A normal sandboxed app sees neither, so the
manifest asks for:

| Permission | Why |
|---|---|
| `--filesystem=host-os` | read `/usr/share/applications` and `/etc/xdg` from the host |
| `--filesystem=home` | read and write `~/.config/mimeapps.list` |

Those are far broader than Flathub's policy allows for a settings app, so this
manifest is intended for **self-distribution**. Distributions and users who
want a sandbox-friendly path should prefer the native packages (AUR, `.deb`,
`.rpm`), which have no sandbox at all.

## How the sandbox is bridged

Flatpak keeps `HOME` pointing at the real home directory but redirects
`XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_CACHE_HOME` into
`~/.var/app/org.salt.default/`. The host OS is mounted under `/run/host`.

`salt-default-wrapper.sh` is installed as the Flatpak `command` and repoints
the XDG variables back at the host (`~/.config`, `~/.local/share`,
`/run/host/usr/share`, …) before exec'ing `salt_default`. Without it the app
would happily "manage" the sandbox's own private MIME database. Verified with
`/proc/<pid>/environ`:

```
XDG_CONFIG_HOME=/home/<user>/.config              <- host, not ~/.var/app/...
XDG_DATA_HOME=/home/<user>/.local/share           <- host
XDG_CONFIG_DIRS=/run/host/etc/xdg:/app/etc/xdg:/etc/xdg
XDG_DATA_DIRS=/run/host/usr/share:/run/host/usr/local/share:/run/host/var/lib/flatpak/exports/share:...
PATH=/app/bin:/usr/bin:/run/host/usr/local/bin:/run/host/usr/bin:/run/host/bin
SALT_HOST_ROOT=/run/host
```

The wrapper also appends the host binary directories to `PATH`. That is not
for launching anything (the app never spawns a process) but for
`GDesktopAppInfo`'s existence check, see below. `SALT_HOST_ROOT` names the
mount point of the host root, which is what an absolute `Icon=` path is
looked up under (the desktop file names a file the host has, not the sandbox):
`Icon=/usr/share/icons/spotube/spotube-logo.png` exists only as
`/run/host/usr/share/icons/spotube/spotube-logo.png` here.

> Debugging note: `flatpak run --command=sh org.salt.default` bypasses the
> wrapper, so the shell sees Flatpak's default XDG values, not the bridged
> ones. Run `--command=sh -c 'exec /app/bin/salt-default-wrapper'` to keep the
> bridge and still get a shell-level view.

## Measured behaviour

Native build versus the Flatpak, both reading the same host database:

| | Apps | File types | Link types |
|---|---|---|---|
| Native | 37 | 1220 | 19 |
| Flatpak | 37 | 1220 | 19 |

The three lists are byte-identical in both environments (each entry was
compared, not just the totals), which is the point of applying one visibility
rule to GIO's app infos and to the directly parsed host desktop files. Two
consequences come with that: the panel hides itself, and a handler that exists
only in the sandbox is not offered — the runtime ships `org.gnome.Yelp.desktop`,
which claims `x-scheme-handler/ghelp`, `help`, `info` and `man`, so those four
link types do not appear, because the host cannot run Yelp. The reverse case is
the host's `antigravity-ide-url-handler.desktop` and
`com.microsoft.VSCode.UrlHandler.desktop` (`NoDisplay=true`): their link types
are hidden on both sides rather than leaving rows the panel cannot offer an
application for.

Also verified inside the sandbox: the host home is writable (a file written by
the app lands in the real `~/.config`), the host `/usr/share/applications` is
readable (51 entries, same as the host), the app logs
`watching /home/<user>/.config/mimeapps.list` — the host path — and the
absolute `Icon=` of `spotube.desktop` resolves through the host root: the log
reports `/usr/share/icons/spotube/spotube-logo.png` as not found and says
nothing about `/run/host/usr/share/icons/spotube/spotube-logo.png`, which is
the candidate that matched.

## Listing host applications whose `Exec` cannot be resolved

GLib's `GioUnix.DesktopAppInfo` refuses to create an app info when the program
named in `TryExec` or in `Exec` cannot be resolved here. From GLib 2.90.0,
`gio/gdesktopappinfo.c`:

```c
t = GLIB_PRIVATE_CALL (g_find_program_for_path) (try_exec, NULL, path);
if (t == NULL)
  return FALSE;              /* no app info */

t = GLIB_PRIVATE_CALL (g_find_program_for_path) (argv[0], NULL, path);
if (t == NULL)
  return FALSE;              /* no app info */
```

Measured inside the sandbox (what the constructor does with a synthetic file):

| File content | Result |
|---|---|
| no `Exec` | app info created |
| `Exec=alacritty` (not in `PATH`) | **no app info** |
| `Exec=/bin/sh` (exists in the sandbox) | app info created |
| `Exec=/usr/bin/alacritty` (missing) | **no app info** |
| `Exec=/run/host/usr/bin/alacritty` (exists via `host-os`) | app info created |
| `TryExec=alacritty` (not in `PATH`) | **no app info** |

Host binaries never exist inside the sandbox, so without help every host
application disappears from GIO's view. Two mechanisms deal with that:

1. **Bare names** (`Exec=firefox`) are resolved through `PATH`, which is why
   the wrapper appends the host binary directories. On its own that recovered
   the list from **3 to 27** applications.
2. **Absolute paths** (`Exec=/usr/bin/google-chrome-stable`) cannot be
   satisfied: `/usr` is the read-only runtime, and the host's `/usr/bin`
   cannot be mounted over it because the host binaries link against host
   libraries that do not exist in the runtime. For those, `load_apps()` scans
   `<data dir>/applications` itself — only when `FLATPAK_ID` is set — and
   builds the entry from the desktop file: desktop file ID, `Name`, `Comment`,
   `Icon`, `Terminal`, `TryExec` and `MimeType`, applying the same visibility
   rules GIO applies (`Type=Application`, `Hidden`, `NoDisplay`,
   `OnlyShowIn`/`NotShowIn`). That took the list from **27 to 37** (38 before the
   panel started filtering its own desktop file out).

Those entries have no `Gio::AppInfo`, so:

- `apps_for_chooser()` adds them to the chooser when their desktop file
  declares the type;
- `Icon=` is resolved from the desktop file rather than taken from GIO: a name
  becomes a themed icon (exactly what GIO builds), an absolute path becomes a
  file icon, and a path that only exists on the host is found again under
  `SALT_HOST_ROOT`. Without that last step `spotube.desktop`
  (`Icon=/usr/share/icons/spotube/spotube-logo.png`) would keep the generic
  fallback icon inside the sandbox;
- `default_app_id()` / `default_label_for()` resolve the default the way the
  host would, which is not simply GIO's answer: for a type whose best
  candidate is a host application, GIO silently skips it and answers with the
  next buildable one. Measured in the sandbox,
  `Gio.AppInfo.get_default_for_type("text/x-csrc")` returns `vim.desktop`
  while `/usr/share/applications/mimeinfo.cache` lists `Helix.desktop` first —
  the host's default really is Helix. The resolution therefore walks the
  user's recorded id, then `mimeinfo.cache` (honouring `[Removed
  Associations]`), and keeps the first candidate it knows GIO had to skip.
  On this host that affects 35 of the 1220 file types: every type whose best
  candidate is a host application (`Helix` for source files, `firefox` for
  JSON/audio/SVG, …). Eight types probed inside the sandbox and natively give
  identical ids and sources:

  | type | default | source |
  |---|---|---|
  | `text/x-csrc` | `Helix.desktop` | system |
  | `text/html` | `google-chrome.desktop` | user |
  | `audio/flac` | `firefox.desktop` | system |
  | `image/svg+xml` | `firefox.desktop` | system |
  | `text/plain` | `Helix.desktop` | system |
  | `application/json` | `firefox.desktop` | system |
  | `text/x-c` | `Helix.desktop` | system |
  | `video/mp4` | `mpv.desktop` | system |

  Only the user's own `~/.config/mimeapps.list` is consulted for removals, so
  a removal declared in a system-wide `mimeapps.list` is not applied by that
  fallback.
- choosing one writes the file directly (`write_user_default()`), reproducing
  what GIO writes — checked against `g_app_info_set_as_default_for_type()`:

  | Section | GIO | this program |
  |---|---|---|
  | `[Default Applications]` | `mime=id` | same |
  | `[Added Associations]` | `mime=id;…;` (accumulated) | same |
  | `[Removed Associations]` | entry for the type dropped | same |

  The test suite in `tests/mime_service_test.cpp` (`just test`) runs the real
  `mime_service.cpp` against a fake XDG environment: the synthetic entry is
  listed while `Hidden`, `NoDisplay`, `OnlyShowIn`, `Type=Link` and nameless
  files are not, `MimeType=` feeds the chooser, the id fallback resolves
  (including a candidate GIO skipped in favour of a buildable one), the
  written file keeps unrelated groups intact, and neither the panel's own
  desktop file nor the types of an invisible application leak into the lists.

This is deliberately sandbox-only: on the host an unresolvable `Exec`/`TryExec`
really does mean the entry cannot be launched, and it stays hidden.

## Warning about the portal monitor

On a host whose portal is incomplete, GTK prints:

```
Gtk-WARNING: Creating a portal monitor failed: ... No such interface
"org.freedesktop.portal.Inhibit" on object at path /org/freedesktop/portal/desktop
```

This is not from salt-default. Checked with
`busctl --user introspect org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop`:
on the tested host that object exposes **no interfaces at all**, so the probe
fails for any GTK application. The native build does not show it only because
`src/main.cpp` sets `GDK_DEBUG=no-portals` on the host. Inside the Flatpak
that workaround is deliberately skipped, because the same switch would also
disable the settings portal that carries the host theme and colour scheme. The
application itself never inhibits or uses portals.

## Updating the bundled bindings

`runtime-version` must be new enough for the bindings on top:

| Module | Version | Needs |
|---|---|---|
| libsigc++ | 3.6.0 | — |
| cairomm | 1.18.1 | sigc++ 3.0 (provides `cairomm-1.16.pc`) |
| glibmm | 2.90.0 | GLib ≥ 2.89.4 (provides `glibmm-2.68.pc`) |
| pangomm | 2.58.0 | cairomm 1.16, glibmm 2.68 (provides `pangomm-2.48.pc`) |
| gtkmm | 4.24.0 | GTK ≥ 4.23.4 |

GNOME 50 ships GTK 4.22 / GLib 2.88, so the set above requires the GNOME **51**
runtime. To target an older runtime, drop `glibmm`/`gtkmm` (and `pangomm` if
needed) to the releases that match it and recompute the `sha256` values.

The GNOME 51 SDK ships GTK 4.24.0, GLib 2.90.0 and GCC 16.2.0; `gtkmm-4.0` is
confirmed absent from it, which is why the stack is built here rather than
taken from the runtime.
