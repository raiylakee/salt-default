// Backend test for MimeService. Runs against a fake XDG environment in
// which GIO can resolve almost nothing, so every answer has to come from the
// sandbox fallback rather than from GIO itself.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unistd.h>

#include <giomm/fileicon.h>
#include <giomm/themedicon.h>
#include <gtkmm/application.h>

#include "app_id.h"
#include "mime_service.h"

namespace fs = std::filesystem;

static int failures = 0;

static void
check (bool ok, const std::string &what)
{
  std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
  if (!ok)
    {
      ++failures;
    }
}

static void
write_file (const fs::path &p, const std::string &body)
{
  fs::create_directories (p.parent_path ());
  std::ofstream f (p);
  f << body;
}

static std::string
read_file (const fs::path &p)
{
  std::ifstream f (p);
  return std::string ((std::istreambuf_iterator<char> (f)),
                      std::istreambuf_iterator<char> ());
}

static auto
contains (const std::vector<salt::MimeTypeEntry> &v, const std::string &mime)
    -> bool
{
  for (const auto &e : v)
    {
      if (e.mime == mime)
        {
          return true;
        }
    }
  return false;
}

int
main ()
{
  // Creating the application object registers the giomm/gtkmm wrapper
  // tables — the real app does the same, and without it a GFileIcon cannot
  // be wrapped.
  auto app = Gtk::Application::create ("org.salt.mime_service_test");

  const fs::path root = fs::temp_directory_path ()
                        / ("salt-default-test-" + std::to_string (getpid ()));
  fs::remove_all (root);

  setenv ("FLATPAK_ID", salt::kAppId, 1);
  setenv ("XDG_CONFIG_HOME", (root / "cfg").c_str (), 1);
  setenv ("XDG_DATA_HOME", (root / "data").c_str (), 1);
  setenv ("XDG_DATA_DIRS", (root / "share").c_str (), 1);
  setenv ("XDG_CURRENT_DESKTOP", "test", 1);

  const fs::path apps = root / "share/applications";

  // Absolute Exec that does not exist here: GIO refuses this file, the
  // fallback must still list it.
  write_file (apps / "hostapp.desktop",
              "[Desktop Entry]\n"
              "Type=Application\n"
              "Name=Host App\n"
              "Comment=A host application\n"
              "Exec=/usr/bin/hostapp %U\n"
              "Icon=hostapp\n"
              "Categories=Network;\n"
              "MimeType=text/html;text/x-csrc;application/x-hostapp-data;"
              "x-scheme-handler/http;x-scheme-handler/zoommtg;\n");

  // Bare Exec that does not exist here: GIO refuses it too.
  write_file (apps / "bareapp.desktop", "[Desktop Entry]\n"
                                        "Type=Application\n"
                                        "Name=Bare App\n"
                                        "Exec=bareapp\n");

  // A resolvable one, so GIO can still answer for a type the host app also
  // claims and the fallback has to overrule it.
  write_file (apps / "shapp.desktop", "[Desktop Entry]\n"
                                      "Type=Application\n"
                                      "Name=Shell App\n"
                                      "Exec=/bin/sh\n"
                                      "MimeType=text/x-csrc;\n");

  // The panel itself: it is buildable here, but must never be offered as a
  // default for anything.
  write_file (apps / (std::string (salt::kAppId) + ".desktop"),
              "[Desktop Entry]\n"
              "Type=Application\n"
              "Name=Salt-Default\n"
              "Exec=/bin/sh\n"
              "MimeType=text/html;\n");

  // GIO's system-default candidate list. text/x-csrc lists the host app
  // first and the buildable one second, which is exactly the case where GIO
  // silently answers with the wrong (buildable) app.
  write_file (apps / "mimeinfo.cache",
              "[MIME Cache]\n"
              "text/html=hostapp.desktop;bareapp.desktop;\n"
              "text/x-csrc=hostapp.desktop;shapp.desktop;\n"
              "application/x-hostapp-data=hostapp.desktop;\n"
              "image/png=hostapp.desktop;\n");

  // Icon= values: a themed name, an existing absolute path, a host path that
  // only exists under the sandbox's host-root mount, and one that is missing.
  write_file (root / "pics/local.png", "stub\n");
  write_file (root / "hostroot/usr/share/icons/hostonly/hostonly.png",
              "stub\n");
  setenv ("SALT_HOST_ROOT", (root / "hostroot").c_str (), 1);
  write_file (apps / "namedicon.desktop",
              "[Desktop Entry]\nType=Application\nName=Named Icon App\n"
              "Exec=/usr/bin/namedicon\nIcon=utilities-terminal\n");
  write_file (apps / "fileicon.desktop",
              "[Desktop Entry]\nType=Application\nName=File Icon App\n"
              "Exec=/usr/bin/fileicon\nIcon="
                  + (root / "pics/local.png").string () + "\n");
  write_file (apps / "hosticon.desktop",
              "[Desktop Entry]\nType=Application\nName=Host Icon App\n"
              "Exec=/usr/bin/hosticon\n"
              "Icon=/usr/share/icons/hostonly/hostonly.png\n");
  write_file (apps / "missingicon.desktop",
              "[Desktop Entry]\nType=Application\nName=Missing Icon App\n"
              "Exec=/usr/bin/missingicon\nIcon=/nowhere/at/all.png\n");

  // Must never be listed, and must not contribute types either.
  write_file (apps / "hidden.desktop",
              "[Desktop Entry]\nType=Application\nName=Hidden\n"
              "Exec=/usr/bin/x\nHidden=true\n"
              "MimeType=x-scheme-handler/hiddenonly;\n");
  write_file (apps / "nodisplay.desktop",
              "[Desktop Entry]\nType=Application\nName=NoDisplay\n"
              "Exec=/usr/bin/nodisplay\nNoDisplay=true\n"
              "MimeType=x-scheme-handler/nodisplayonly;"
              "application/x-nodisplay-only;\n");
  write_file (apps / "wrongde.desktop",
              "[Desktop Entry]\nType=Application\nName=Wrong Desktop\n"
              "Exec=/usr/bin/wrongde\nOnlyShowIn=GNOME;\n"
              "MimeType=x-scheme-handler/otherdesktop;\n");
  write_file (apps / "nameless.desktop",
              "[Desktop Entry]\nType=Application\nExec=/usr/bin/x\n");
  write_file (apps / "linktype.desktop",
              "[Desktop Entry]\nType=Link\nName=A Link\n"
              "URL=https://example.com\n");

  std::cout << "=== load_apps(): fallback entries ===\n";
  salt::MimeService service;
  auto apps_list = service.load_apps ();

  const salt::AppEntry *host = nullptr;
  const salt::AppEntry *bare = nullptr;
  for (const auto &a : apps_list)
    {
      if (a.id == "hostapp.desktop")
        {
          host = &a;
        }
      if (a.id == "bareapp.desktop")
        {
          bare = &a;
        }
    }

  check (host != nullptr, "hostapp.desktop (absolute Exec) is listed");
  check (bare != nullptr, "bareapp.desktop (bare Exec) is listed");
  if (host != nullptr)
    {
      check (host->synthetic, "hostapp is marked synthetic");
      check (host->name == "Host App", "name comes from the desktop file");
      check (host->description == "A host application", "Comment is read");
      check (host->desktop_file == (apps / "hostapp.desktop").string (),
             "desktop_file is the real path");
      check (host->icon != nullptr, "Icon= produced an icon");
      check (host->supported_types.size () == 5,
             "MimeType= parsed into 5 supported types");
    }
  for (const auto &a : apps_list)
    {
      if (a.id == "hidden.desktop" || a.id == "nodisplay.desktop"
          || a.id == "wrongde.desktop" || a.id == "nameless.desktop"
          || a.id == "linktype.desktop")
        {
          check (false, "excluded entry leaked into the list: " + a.id);
        }
    }
  {
    std::set<std::string> ids;
    bool duplicate = false;
    for (const auto &a : apps_list)
      {
        duplicate = duplicate || !ids.insert (a.id).second;
      }
    check (!duplicate, "no entry is listed twice");
  }

  std::cout << "=== the panel itself is not a choosable default ===\n";
  {
    const std::string self = std::string (salt::kAppId) + ".desktop";
    bool self_listed = false;
    for (const auto &a : apps_list)
      {
        self_listed = self_listed || a.id == self;
      }
    check (!self_listed, self + " is not listed");
    bool self_offered = false;
    for (const auto &a : service.apps_for_chooser ("text/html"))
      {
        self_offered = self_offered || a.id == self;
      }
    check (!self_offered, self + " is not offered for a type it claims");
  }
  std::cout << "  (total entries: " << apps_list.size () << ")\n";

  std::cout << "=== Icon= resolution ===\n";
  {
    const auto find
        = [&apps_list] (const std::string &id) -> const salt::AppEntry *
      {
        for (const auto &a : apps_list)
          {
            if (a.id == id)
              {
                return &a;
              }
          }
        return nullptr;
      };
    const auto icon_path = [] (const salt::AppEntry *e)
      {
        if (e == nullptr || !e->icon)
          {
            return std::string ("<none>");
          }
        if (auto fi = std::dynamic_pointer_cast<Gio::FileIcon> (e->icon))
          {
            auto file = fi->get_file ();
            return file ? file->get_path () : std::string ("<no path>");
          }
        if (std::dynamic_pointer_cast<Gio::ThemedIcon> (e->icon))
          {
            return std::string ("<themed>");
          }
        return std::string ("<other>");
      };

    check (icon_path (find ("namedicon.desktop")) == "<themed>",
           "a name Icon= becomes a themed icon");
    check (icon_path (find ("fileicon.desktop"))
               == (root / "pics/local.png").string (),
           "an existing absolute Icon= becomes a file icon");
    check (icon_path (find ("hosticon.desktop"))
               == (root / "hostroot/usr/share/icons/hostonly/hostonly.png")
                      .string (),
           "a host-only path is found under SALT_HOST_ROOT");
    check (icon_path (find ("missingicon.desktop")) == "<none>",
           "an Icon= that exists nowhere keeps the generic fallback");
  }

  std::cout << "=== types claimed only by a skipped host app ===\n";
  {
    const auto file_types = service.load_file_types ();
    check (contains (file_types, "application/x-hostapp-data"),
           "a custom file type of a skipped host app is listed");
    check (contains (file_types, "text/x-csrc"),
           "a type both the host app and a buildable app claim is listed");
    const auto schemes = service.load_schemes ();
    check (contains (schemes, "x-scheme-handler/zoommtg"),
           "a scheme only a skipped host app claims is listed");
    check (contains (schemes, "x-scheme-handler/http"),
           "a well-known scheme is listed");
    check (!contains (schemes, "x-scheme-handler/nodisplayonly"),
           "a NoDisplay app does not contribute a scheme");
    check (!contains (schemes, "x-scheme-handler/hiddenonly"),
           "a Hidden app does not contribute a scheme");
    check (!contains (schemes, "x-scheme-handler/otherdesktop"),
           "an OnlyShowIn=other-desktop app does not contribute a scheme");
    check (!contains (file_types, "application/x-nodisplay-only"),
           "a NoDisplay app does not contribute a file type");
  }

  std::cout << "=== apps_for_chooser() ===\n";
  auto chooser = service.apps_for_chooser ("text/html");
  bool host_offered = false;
  for (const auto &a : chooser)
    {
      host_offered = host_offered || a.id == "hostapp.desktop";
    }
  check (host_offered, "hostapp is offered for text/html");

  auto chooser_png = service.apps_for_chooser ("image/png");
  bool host_offered_png = false;
  for (const auto &a : chooser_png)
    {
      host_offered_png = host_offered_png || a.id == "hostapp.desktop";
    }
  check (!host_offered_png,
         "hostapp is NOT offered for a type it does not declare");

  std::cout << "=== set_default() through the mimeapps.list writer ===\n";
  const fs::path mimeapps = root / "cfg/mimeapps.list";
  // Pre-existing content that must survive, plus a Removed Associations
  // entry for the very app we are about to choose.
  write_file (mimeapps, "[Removed Associations]\n"
                        "text/html=hostapp.desktop;\n"
                        "image/png=hostapp.desktop;\n"
                        "\n"
                        "[Default Applications]\n"
                        "image/png=hostapp.desktop\n"
                        "\n"
                        "[Added Associations]\n"
                        "image/png=hostapp.desktop;\n");

  std::string err;
  salt::AppEntry copy = *host;
  const bool ok = service.set_default (copy, "text/html", err);
  check (ok, "set_default(text/html) succeeded" + (ok ? "" : ": " + err));

  const std::string after = read_file (mimeapps);
  check (after.find ("text/html=hostapp.desktop") != std::string::npos,
         "[Default Applications] got the id");
  check (after.find ("[Added Associations]") != std::string::npos
             && after.find ("text/html=hostapp.desktop;") != std::string::npos,
         "[Added Associations] got the id");
  // The Removed Associations entry for text/html must be gone, but the
  // unrelated image/png one must stay.
  const auto removed_pos = after.find ("[Removed Associations]");
  const auto default_pos = after.find ("[Default Applications]");
  const std::string removed_block
      = removed_pos == std::string::npos
            ? std::string ()
            : after.substr (removed_pos, default_pos - removed_pos);
  check (removed_block.find ("text/html=") == std::string::npos,
         "text/html was dropped from [Removed Associations]");
  check (removed_block.find ("image/png=hostapp.desktop;")
             != std::string::npos,
         "the unrelated [Removed Associations] entry was preserved");
  check (after.find ("image/png=hostapp.desktop") != std::string::npos,
         "the unrelated default was preserved");

  std::cout << "=== user_default_id() / default_app_id() ===\n";
  check (service.user_default_id ("text/html") == "hostapp.desktop",
         "user_default_id reads what we just wrote");
  check (service.default_app_id ("text/html") == "hostapp.desktop",
         "default_app_id falls back to the recorded id");

  std::cout << "=== GIO's fallthrough versus the host's system default ===\n";
  {
    std::string gio_id;
    if (auto def = service.default_for ("text/x-csrc"))
      {
        gio_id = def->get_id ();
      }
    check (gio_id == "shapp.desktop",
           "GIO skipped the host app and answered with the buildable one"
           " (got '"
               + (gio_id.empty () ? std::string ("<none>") : gio_id) + "')");
    check (service.default_app_id ("text/x-csrc") == "hostapp.desktop",
           "the skipped host candidate is still reported as the default");
    check (service.default_source ("text/x-csrc") == "system",
           "and counted as a system default");
    check (service.default_app_id ("application/x-hostapp-data")
               == "hostapp.desktop",
           "a type only the host app claims resolves as well");
    check (service.default_source ("application/x-hostapp-data") == "system",
           "with a system source");
    check (service.default_app_id ("application/zip").empty (),
           "a type nobody claims stays empty");
    check (service.default_source ("application/zip") == "none",
           "and is reported as no default");
  }

  std::cout << "=== [Removed Associations] participates in the walk ===\n";
  {
    write_file (mimeapps, "[Removed Associations]\n"
                          "text/x-csrc=hostapp.desktop;\n"
                          "\n"
                          "[Default Applications]\n"
                          "\n"
                          "[Added Associations]\n");
    salt::MimeService fresh;
    check (fresh.default_app_id ("text/x-csrc") == "shapp.desktop",
           "the first surviving candidate becomes the default");
    check (fresh.default_source ("text/x-csrc") == "system",
           "and it is still a system default");
  }

  fs::remove_all (root);

  std::cout << "\n";
  if (failures != 0)
    {
      std::cout << "FAILURES: " << failures << "\n";
      return 1;
    }
  std::cout << "ALL CHECKS PASSED\n";
  return 0;
}
