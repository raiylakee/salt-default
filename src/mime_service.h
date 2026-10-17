#pragma once

#include <giomm/appinfo.h>
#include <giomm/icon.h>
#include <glibmm/ustring.h>

#include <map>
#include <set>
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
  // Resolved icon; gapp has none for a synthetic entry.
  Glib::RefPtr<Gio::Icon> icon;
  std::vector<std::string> supported_types;
  bool terminal = false;
  std::string try_exec;
  std::string desktop_file;
  // True when GIO refused to build an app info for this desktop file
  // (Exec=/TryExec= not resolvable here — inside a Flatpak that is every host
  // application, which the host can still launch), so the entry was built
  // from the desktop file instead.
  bool synthetic = false;
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

  // Chooser candidates: GIO's list plus the synthetic entries for the type.
  std::vector<AppEntry> apps_for_chooser (const std::string &mime);

  bool set_default (const Glib::RefPtr<Gio::AppInfo> &app,
                    const std::string &mime, std::string &error_out);
  // Same for a synthetic entry, which writes mimeapps.list itself.
  bool set_default (const AppEntry &app, const std::string &mime,
                    std::string &error_out);
  bool reset_to_system (const std::string &mime, std::string &error_out);

  static std::string xdg_data_home ();
  static std::vector<std::string> xdg_data_dirs ();
  static std::string user_config_path ();
  static bool try_exec_available (const std::string &try_exec);

  // True when ~/.config/mimeapps.list [Default Applications] overrides mime.
  bool is_user_default (const std::string &mime);
  std::string default_source (const std::string &mime);
  // Desktop id of the default for mime, the way the host would resolve it:
  // GIO skips the candidates it cannot build and answers with the next one,
  // so the user's own id and mimeinfo.cache are consulted as well.
  std::string default_app_id (const std::string &mime);
  std::string user_default_id (const std::string &mime);

  // XDG [Removed Associations]: apps the user disallowed for a type.
  std::vector<std::string> removed_associations_for (const std::string &mime);
  bool is_association_removed (const std::string &app_id,
                               const std::string &mime);
  bool remove_association (const std::string &app_id, const std::string &mime,
                           std::string &error_out);
  bool restore_association (const std::string &app_id, const std::string &mime,
                            std::string &error_out);

  // Drop the parsed ~/.config/mimeapps.list cache (after external writes).
  void invalidate_mimeapps_cache ();

  static bool is_scheme_mime (const std::string &mime);
  static std::string scheme_name (const std::string &mime);
  std::string describe (const std::string &mime);
  std::string extensions_for (const std::string &mime);

private:
  // Desktop files in the XDG applications directories that GIO refused.
  static std::vector<AppEntry> synthetic_apps ();
  static std::vector<AppEntry>
  scan_skipped_desktop_files (const std::set<std::string> &known_ids);
  static AppEntry entry_from_app_info (const Glib::RefPtr<Gio::AppInfo> &info);

  // Desktop ids of the synthetic entries. GIO's default lookup skips those,
  // which is how default_app_id() spots a fallthrough.
  void ensure_synthetic_ids_loaded ();
  bool synthetic_ids_loaded_ = false;
  std::set<std::string> synthetic_ids_;

  // First candidate per type from applications/mimeinfo.cache — the system
  // default when no mimeapps.list entry exists. Parsed once.
  void ensure_mimeinfo_loaded ();
  bool mimeinfo_loaded_ = false;
  std::map<std::string, std::vector<std::string>> mimeinfo_;
  std::string system_default_id (const std::string &mime);

  // Writes ~/.config/mimeapps.list the way GIO would.
  bool write_user_default (const std::string &desktop_id,
                           const std::string &mime, std::string &error_out);

  void ensure_globs_loaded ();
  bool globs_loaded_ = false;
  std::map<std::string, std::vector<std::string>> globs_;

  // Parsed once per change instead of re-reading the file per UI row.
  void ensure_mimeapps_loaded ();
  bool mimeapps_loaded_ = false;
  std::map<std::string, std::string> user_defaults_;
  std::map<std::string, std::vector<std::string>> removed_associations_;
};

} // namespace salt
