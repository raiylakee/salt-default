#include "mime_service.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include <giomm/contenttype.h>
#include <giomm/desktopappinfo.h>
#include <glibmm/error.h>

#include "log.h"

namespace salt
{

namespace fs = std::filesystem;

namespace
{

auto
to_lower (std::string s) -> std::string
{
  std::transform (s.begin (), s.end (), s.begin (),
                  [] (unsigned char c) { return std::tolower (c); });
  return s;
}

auto
trim (std::string s) -> std::string
{
  const auto not_space = [] (unsigned char c) { return !std::isspace (c); };
  s.erase (s.begin (), std::find_if (s.begin (), s.end (), not_space));
  s.erase (std::find_if (s.rbegin (), s.rend (), not_space).base (), s.end ());
  return s;
}

auto
split_colon (const std::string &s) -> std::vector<std::string>
{
  std::vector<std::string> out;
  std::string cur;
  std::istringstream ss (s);
  while (std::getline (ss, cur, ':'))
    {
      cur = trim (cur);
      if (!cur.empty ())
        {
          out.push_back (cur);
        }
    }
  return out;
}

} // namespace

MimeService::MimeService () = default;

auto
MimeService::is_scheme_mime (const std::string &mime) -> bool
{
  constexpr std::string_view prefix = "x-scheme-handler/";
  return mime.rfind (prefix.data (), 0) == 0;
}

auto
MimeService::scheme_name (const std::string &mime) -> std::string
{
  constexpr std::string_view prefix = "x-scheme-handler/";
  if (is_scheme_mime (mime))
    {
      return mime.substr (prefix.size ());
    }
  return mime;
}

auto
MimeService::xdg_data_home () -> std::string
{
  if (const char *env = std::getenv ("XDG_DATA_HOME");
      env != nullptr && *env != '\0')
    {
      return env;
    }
  const char *home = std::getenv ("HOME");
  if (home == nullptr || *home == '\0')
    {
      return "/usr/share";
    }
  return fs::path (home) / ".local" / "share";
}

auto
MimeService::xdg_data_dirs () -> std::vector<std::string>
{
  // XDG Base Directory spec + Flatpak exports come through these env vars,
  // so honoring them keeps globs/extensions correct inside Flatpak/host.
  std::vector<std::string> dirs;
  dirs.push_back (xdg_data_home ());

  const char *env = std::getenv ("XDG_DATA_DIRS");
  std::string raw
      = (env != nullptr && *env != '\0') ? env : "/usr/local/share:/usr/share";
  for (auto &d : split_colon (raw))
    {
      if (std::find (dirs.begin (), dirs.end (), d) == dirs.end ())
        {
          dirs.push_back (d);
        }
    }
  return dirs;
}

auto
MimeService::user_config_path () -> std::string
{
  const char *xdg = std::getenv ("XDG_CONFIG_HOME");
  fs::path base;
  if (xdg != nullptr && *xdg != '\0')
    {
      base = xdg;
    }
  else
    {
      const char *home = std::getenv ("HOME");
      base = fs::path (home != nullptr ? home : "/") / ".config";
    }
  return base / "mimeapps.list";
}

auto
MimeService::try_exec_available (const std::string &try_exec) -> bool
{
  std::string prog = trim (try_exec);
  if (prog.empty ())
    {
      return true;
    }
  // TryExec may contain args; only the binary matters per Desktop Entry spec.
  auto sp = prog.find (' ');
  if (sp != std::string::npos)
    {
      prog = prog.substr (0, sp);
    }
  if (prog.find ('/') != std::string::npos)
    {
      return ::access (prog.c_str (), X_OK) == 0;
    }
  const char *path_env = std::getenv ("PATH");
  std::string path_raw = path_env != nullptr ? path_env : "/usr/bin:/bin";
  std::string cur;
  std::istringstream ss (path_raw);
  while (std::getline (ss, cur, ':'))
    {
      if (cur.empty ())
        {
          continue;
        }
      if (::access ((fs::path (cur) / prog).c_str (), X_OK) == 0)
        {
          return true;
        }
    }
  return false;
}

void
MimeService::ensure_globs_loaded ()
{
  if (globs_loaded_)
    {
      return;
    }
  globs_loaded_ = true;

  for (auto &dir : xdg_data_dirs ())
    {
      fs::path globs = fs::path (dir) / "mime" / "globs";
      std::ifstream file (globs);
      if (!file.is_open ())
        {
          continue;
        }
      std::string line;
      while (std::getline (file, line))
        {
          if (line.empty () || line[0] == '#')
            {
              continue;
            }
          auto sep = line.find (':');
          if (sep == std::string::npos)
            {
              continue;
            }
          std::string mime = line.substr (0, sep);
          std::string pattern = line.substr (sep + 1);
          if (!pattern.empty () && pattern[0] == '*')
            {
              // keep patterns like *.html / *.pdf
              auto &vec = globs_[mime];
              if (std::find (vec.begin (), vec.end (), pattern) == vec.end ())
                {
                  if (vec.size () < 4)
                    {
                      vec.push_back (pattern);
                    }
                }
            }
        }
    }
}

auto
MimeService::describe (const std::string &mime) -> std::string
{
  if (is_scheme_mime (mime))
    {
      return scheme_name (mime) + " link";
    }
  try
    {
      Glib::ustring desc = Gio::content_type_get_description (mime);
      if (!desc.empty ())
        {
          return desc;
        }
    }
  catch (const Glib::Error &e)
    {
      spdlog::debug ("no description for {}: {}", mime, e.what ());
    }
  return mime;
}

auto
MimeService::extensions_for (const std::string &mime) -> std::string
{
  if (is_scheme_mime (mime))
    {
      return scheme_name (mime) + ":";
    }
  ensure_globs_loaded ();
  auto it = globs_.find (mime);
  if (it == globs_.end () || it->second.empty ())
    {
      return mime;
    }
  std::string out;
  for (size_t i = 0; i < it->second.size (); ++i)
    {
      if (i > 0)
        {
          out += ", ";
        }
      out += it->second[i];
    }
  return out;
}

auto
MimeService::load_apps () -> std::vector<AppEntry>
{
  std::vector<AppEntry> apps;
  auto all = Gio::AppInfo::get_all ();
  for (auto &info : all)
    {
      if (!info)
        {
          continue;
        }
      // should_show() covers NoDisplay/OnlyShowIn/NotShowIn.
      bool show = true;
      try
        {
          show = info->should_show ();
        }
      catch (...)
        {
          show = true;
        }
      if (!show)
        {
          continue;
        }
      // Desktop Entry spec extras Gio::AppInfo::should_show() misses:
      // Hidden=true must never appear; TryExec pointing at a missing
      // binary means the app cannot launch here (common with stale
      // .desktop files and partial Flatpak exports).
      auto desktop = std::dynamic_pointer_cast<Gio::DesktopAppInfo> (info);
      if (desktop)
        {
          try
            {
              if (desktop->is_hidden ())
                {
                  continue;
                }
            }
          catch (...)
            {
            }
          try
            {
              if (desktop->has_key ("TryExec")
                  && !try_exec_available (
                      std::string (desktop->get_string ("TryExec"))))
                {
                  spdlog::debug ("hiding {}: TryExec missing",
                                 info->get_id ());
                  continue;
                }
            }
          catch (...)
            {
            }
        }
      AppEntry e;
      e.gapp = info;
      try
        {
          e.id = info->get_id ();
        }
      catch (...)
        {
          continue;
        }
      if (e.id.empty ())
        {
          continue;
        }
      try
        {
          e.name = info->get_name ();
        }
      catch (...)
        {
          e.name = e.id;
        }
      try
        {
          e.description = info->get_description ();
        }
      catch (...)
        {
        }
      try
        {
          auto types = info->get_supported_types ();
          for (auto &t : types)
            {
              e.supported_types.emplace_back (t);
            }
        }
      catch (...)
        {
        }
      if (desktop)
        {
          try
            {
              if (desktop->has_key ("TryExec"))
                {
                  e.try_exec = desktop->get_string ("TryExec");
                }
              e.terminal = desktop->has_key ("Terminal")
                               ? desktop->get_boolean ("Terminal")
                               : false;
              e.desktop_file = desktop->get_filename ();
            }
          catch (...)
            {
            }
        }
      if (e.name.empty ())
        {
          e.name = e.id;
        }
      apps.push_back (std::move (e));
    }

  std::sort (
      apps.begin (), apps.end (), [] (const AppEntry &a, const AppEntry &b)
        { return to_lower (a.name.raw ()) < to_lower (b.name.raw ()); });
  return apps;
}

auto
MimeService::load_file_types () -> std::vector<MimeTypeEntry>
{
  std::vector<std::string> mimes;
  try
    {
      auto registered = Gio::content_types_get_registered ();
      for (auto &t : registered)
        {
          Glib::ustring mt = Gio::content_type_get_mime_type (t);
          std::string mime = mt.empty () ? std::string (t) : std::string (mt);
          if (!is_scheme_mime (mime))
            {
              mimes.push_back (mime);
            }
        }
    }
  catch (const Glib::Error &e)
    {
      spdlog::warn ("content_types_get_registered failed: {}", e.what ());
    }

  // Always include types claimed by apps even if not in the
  // XDG mime database (e.g. custom types).
  for (auto &app : Gio::AppInfo::get_all ())
    {
      if (!app)
        {
          continue;
        }
      try
        {
          for (auto &t : app->get_supported_types ())
            {
              std::string mime (t);
              if (!is_scheme_mime (mime)
                  && std::find (mimes.begin (), mimes.end (), mime)
                         == mimes.end ())
                {
                  mimes.push_back (mime);
                }
            }
        }
      catch (...)
        {
        }
    }

  std::sort (mimes.begin (), mimes.end ());
  mimes.erase (std::unique (mimes.begin (), mimes.end ()), mimes.end ());

  std::vector<MimeTypeEntry> out;
  out.reserve (mimes.size ());
  for (auto &m : mimes)
    {
      MimeTypeEntry e;
      e.mime = m;
      e.is_scheme = false;
      e.description = describe (m);
      e.extensions_label = extensions_for (m);
      out.push_back (std::move (e));
    }
  return out;
}

auto
MimeService::load_schemes () -> std::vector<MimeTypeEntry>
{
  std::vector<std::string> mimes;
  for (auto &app : Gio::AppInfo::get_all ())
    {
      if (!app)
        {
          continue;
        }
      try
        {
          for (auto &t : app->get_supported_types ())
            {
              std::string mime (t);
              if (is_scheme_mime (mime)
                  && std::find (mimes.begin (), mimes.end (), mime)
                         == mimes.end ())
                {
                  mimes.push_back (mime);
                }
            }
        }
      catch (...)
        {
        }
    }
  // Include well-known schemes even when no app claims them so the
  // user can still assign one (mirrors Windows link-type list).
  for (const char *known : { "x-scheme-handler/http", "x-scheme-handler/https",
                             "x-scheme-handler/mailto", "x-scheme-handler/ftp",
                             "x-scheme-handler/file" })
    {
      if (std::find (mimes.begin (), mimes.end (), known) == mimes.end ())
        {
          mimes.emplace_back (known);
        }
    }

  std::sort (mimes.begin (), mimes.end ());
  std::vector<MimeTypeEntry> out;
  for (auto &m : mimes)
    {
      MimeTypeEntry e;
      e.mime = m;
      e.is_scheme = true;
      e.scheme = scheme_name (m);
      e.description = describe (m);
      e.extensions_label = extensions_for (m);
      out.push_back (std::move (e));
    }
  return out;
}

auto
MimeService::default_for (const std::string &mime)
    -> Glib::RefPtr<Gio::AppInfo>
{
  try
    {
      return Gio::AppInfo::get_default_for_type (mime, false);
    }
  catch (const Glib::Error &e)
    {
      spdlog::debug ("get_default_for_type({}) failed: {}", mime, e.what ());
      return {};
    }
}

auto
MimeService::apps_for (const std::string &mime)
    -> std::vector<Glib::RefPtr<Gio::AppInfo>>
{
  try
    {
      return Gio::AppInfo::get_all_for_type (mime);
    }
  catch (const Glib::Error &e)
    {
      spdlog::debug ("get_all_for_type({}) failed: {}", mime, e.what ());
      return {};
    }
}

auto
MimeService::set_default (const Glib::RefPtr<Gio::AppInfo> &app,
                          const std::string &mime, std::string &error_out)
    -> bool
{
  if (!app)
    {
      error_out = "no application selected";
      return false;
    }
  try
    {
      // Writes ~/.config/mimeapps.list [Default Applications].
      bool ok = app->set_as_default_for_type (mime);
      if (!ok)
        {
          error_out = "backend reported failure";
          return false;
        }
      return true;
    }
  catch (const Glib::Error &e)
    {
      error_out = e.what ();
      spdlog::error ("set_as_default_for_type({}) failed: {}", mime,
                     error_out);
      return false;
    }
}

auto
MimeService::reset_to_system (const std::string &mime, std::string &error_out)
    -> bool
{
  try
    {
      // Removes the [Default Applications] override so the system default
      // (defaults.list / mimeinfo.cache) applies again. Per spec this may
      // add a [Removed Associations] entry via GIO.
      Gio::AppInfo::reset_type_associations (mime);
      return true;
    }
  catch (const Glib::Error &e)
    {
      error_out = e.what ();
      spdlog::error ("reset_type_associations({}) failed: {}", mime,
                     error_out);
      return false;
    }
  catch (const std::exception &e)
    {
      error_out = e.what ();
      return false;
    }
}

auto
MimeService::is_user_default (const std::string &mime) -> bool
{
  // Parse only the user file; Gio merges system files underneath, so an
  // entry here means "user override", absence means "system / none".
  std::ifstream file (user_config_path ());
  if (!file.is_open ())
    {
      return false;
    }
  bool in_defaults = false;
  std::string line;
  while (std::getline (file, line))
    {
      std::string t = trim (line);
      if (t.empty () || t[0] == '#')
        {
          continue;
        }
      if (t.front () == '[' && t.back () == ']')
        {
          in_defaults = (t == "[Default Applications]");
          continue;
        }
      if (!in_defaults)
        {
          continue;
        }
      auto eq = t.find ('=');
      if (eq == std::string::npos)
        {
          continue;
        }
      if (trim (t.substr (0, eq)) == mime)
        {
          return true;
        }
    }
  return false;
}

auto
MimeService::default_source (const std::string &mime) -> std::string
{
  if (is_user_default (mime))
    {
      return "user";
    }
  return default_for (mime) ? "system" : "none";
}

} // namespace salt
