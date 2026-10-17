#pragma once

#include <giomm/appinfo.h>
#include <giomm/icon.h>
#include <glibmm/ustring.h>

#include <map>
#include <string>
#include <vector>

namespace salt
{

struct AppEntry
{
  std::string id;
  Glib::ustring name;
  Glib::ustring description;
  Glib::RefPtr<Gio::AppInfo> gapp;
  std::vector<std::string> supported_types;
  bool terminal = false;
  std::string try_exec;
  std::string desktop_file;
};

struct MimeTypeEntry
{
  std::string mime;
  std::string description;
  std::string extensions_label;
  bool is_scheme = false;
  std::string scheme;
};

class MimeService
{
public:
  MimeService ();

  std::vector<AppEntry> load_apps ();
  std::vector<MimeTypeEntry> load_file_types ();
  std::vector<MimeTypeEntry> load_schemes ();

  Glib::RefPtr<Gio::AppInfo> default_for (const std::string &mime);
  std::vector<Glib::RefPtr<Gio::AppInfo>> apps_for (const std::string &mime);

  bool set_default (const Glib::RefPtr<Gio::AppInfo> &app,
                    const std::string &mime, std::string &error_out);
  bool reset_to_system (const std::string &mime, std::string &error_out);

  // XDG base directories (spec-compliant, Flatpak-aware via env).
  static std::string xdg_data_home ();
  static std::vector<std::string> xdg_data_dirs ();
  static std::string user_config_path ();
  static bool try_exec_available (const std::string &try_exec);

  // True when ~/.config/mimeapps.list [Default Applications] overrides mime.
  bool is_user_default (const std::string &mime);
  std::string default_source (const std::string &mime);

  static bool is_scheme_mime (const std::string &mime);
  static std::string scheme_name (const std::string &mime);
  std::string describe (const std::string &mime);
  std::string extensions_for (const std::string &mime);

private:
  void ensure_globs_loaded ();
  bool globs_loaded_ = false;
  std::map<std::string, std::vector<std::string>> globs_;
};

} // namespace salt
