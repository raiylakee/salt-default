#!/bin/sh
#
# Flatpak entry point for salt-default.
#
# Inside a Flatpak sandbox the XDG directories point at the app's private
# copy (~/.var/app/org.salt.default/{config,data}), and the host OS lives
# under /run/host. A "default applications" panel has to read and write the
# *host* MIME database, so this wrapper repoints the XDG variables back at
# the host before starting the application.

set -e

if [ -n "${FLATPAK_ID:-}" ]; then
    : "${HOME:?HOME is not set}"

    XDG_CONFIG_HOME="${HOME}/.config"
    XDG_DATA_HOME="${HOME}/.local/share"
    XDG_CONFIG_DIRS="/run/host/etc/xdg${XDG_CONFIG_DIRS:+:${XDG_CONFIG_DIRS}}"
    XDG_DATA_DIRS="/run/host/usr/share:/run/host/usr/local/share:/run/host/var/lib/flatpak/exports/share${XDG_DATA_DIRS:+:${XDG_DATA_DIRS}}"

    # GLib's DesktopAppInfo only creates an app info when the program named
    # in Exec=/TryExec= can be resolved, so every host application whose
    # binary lives outside the sandbox would be invisible. The host
    # binaries are already mounted read-only by --filesystem=host-os;
    # appending them to PATH satisfies that check. Appended, not
    # prepended, so the runtime's own binaries keep precedence in the
    # (unlikely) event that anything here gets executed.
    PATH="${PATH}:/run/host/usr/local/bin:/run/host/usr/bin:/run/host/bin"

    # Where this sandbox mounts the host root. An absolute Icon= path in a
    # host desktop file names a file only the host has at that location, so
    # the application looks it up under this prefix as well.
    SALT_HOST_ROOT="/run/host"

    export XDG_CONFIG_HOME XDG_DATA_HOME XDG_CONFIG_DIRS XDG_DATA_DIRS PATH
    export SALT_HOST_ROOT
fi

exec salt_default "$@"
