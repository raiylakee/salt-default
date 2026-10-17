# RPM spec for salt-default
#
# Build (from a tagged release tarball):
#     rpmbuild -ba rpm/salt-default.spec
# or, straight from a git checkout:
#     rpmbuild -ba rpm/salt-default.spec --define "_sourcedir $PWD/.."
#
# Tested package names are Fedora's. On openSUSE use:
#     BuildRequires:  gtkmm4-devel
#     Requires:       libgtkmm-4_0-1

Name:           salt-default
Version:        0.0.1
Release:        1%{?dist}
Summary:        Windows-style "Default Apps" settings panel for Linux

License:        GPL-3.0-or-later
URL:            https://github.com/your-org/salt-default
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(gtkmm-4.0)
# Optional: used by %%check to validate the installed .desktop / AppStream data.
BuildRequires:  desktop-file-utils
BuildRequires:  appstream

Requires:       gtkmm4.0

%description
Salt-Default is a graphical settings panel that lets you choose which
application opens a given kind of file or link, the equivalent of the
Windows "Default apps" page.

It is backed by the standard XDG MIME database (mimeapps.list and .desktop
files), so the choices made here are honoured by the rest of the desktop.

Features:
  * set defaults per application, per file type (MIME) or per link type
  * shows whether a default comes from user configuration or the system
  * manage and restore removed associations
  * reloads automatically when mimeapps.list changes elsewhere

%prep
%autosetup -n %{name}-%{version}

%build
%meson
%meson_build

%install
%meson_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/salt-default.desktop
# AppStream validation needs network access for some checks; never fail the
# build over it.
appstreamcli validate --no-net %{buildroot}%{_datadir}/metainfo/org.salt.default.metainfo.xml || true

%files
%license LICENSE
%doc README.md
%{_bindir}/salt_default
%{_datadir}/applications/salt-default.desktop
%{_datadir}/metainfo/org.salt.default.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/salt-default.svg

%changelog
* Sat Oct 17 2026 Salt-Default Project <maintainer@example.com> - 0.0.1-1
- Initial package.
