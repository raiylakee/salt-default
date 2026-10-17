#include "app_window.h"

#include <algorithm>
#include <cctype>

#include <giomm/contenttype.h>

#include "log.h"

namespace salt
{

MainWindow::MainWindow ()
    : root_ (Gtk::Orientation::VERTICAL), body_ (Gtk::Orientation::HORIZONTAL),
      apps_page_ (Gtk::Orientation::HORIZONTAL),
      apps_paned_ (Gtk::Orientation::HORIZONTAL),
      apps_left_ (Gtk::Orientation::VERTICAL),
      detail_box_ (Gtk::Orientation::VERTICAL),
      detail_content_ (Gtk::Orientation::VERTICAL),
      detail_header_ (Gtk::Orientation::HORIZONTAL),
      detail_titles_ (Gtk::Orientation::VERTICAL),
      filetypes_page_ (Gtk::Orientation::VERTICAL),
      schemes_page_ (Gtk::Orientation::VERTICAL)
{
  set_title ("Default Apps");
  set_default_size (980, 640);

  build_shell ();
  build_apps_page ();
  build_filetypes_page ();
  build_schemes_page ();

  reload_all ();

  start_config_watch ();

  if (!apps_.empty ())
    {
      show_app_detail (apps_.front ().id);
    }
}

void
MainWindow::start_config_watch ()
{
  try
    {
      std::string cfg = MimeService::user_config_path ();
      config_file_ = Gio::File::create_for_path (cfg);
      auto parent = config_file_->get_parent ();
      if (parent)
        {
          config_dir_ = parent;
          // Watch the directory: covers create/replace/delete of
          // mimeapps.list, which GIO does atomically.
          config_dir_monitor_ = config_dir_->monitor_directory ();
          config_dir_monitor_->signal_changed ().connect (
              [this] (const Glib::RefPtr<Gio::File> &file,
                      const Glib::RefPtr<Gio::File> &, Gio::FileMonitor::Event)
                {
                  if (file && file->get_basename () == "mimeapps.list")
                    {
                      schedule_config_reload ();
                    }
                });
        }
      // Best-effort direct file watch as well (exists on most systems).
      config_file_monitor_ = config_file_->monitor_file ();
      config_file_monitor_->signal_changed ().connect (
          [this] (const Glib::RefPtr<Gio::File> &,
                  const Glib::RefPtr<Gio::File> &, Gio::FileMonitor::Event)
            { schedule_config_reload (); });
      spdlog::info ("watching {}", cfg);
    }
  catch (const Glib::Error &e)
    {
      spdlog::warn ("config watch unavailable: {}", e.what ());
    }
}

void
MainWindow::schedule_config_reload ()
{
  // Debounce: editors/portals can emit bursts; reload once settled.
  if (config_reload_timeout_.connected ())
    {
      config_reload_timeout_.disconnect ();
    }
  config_reload_timeout_ = Glib::signal_timeout ().connect (
      [this] ()
        {
          spdlog::info ("mimeapps.list changed externally, reloading");
          reload_all ();
          refresh_current_views ();
          set_status ("Reloaded — mimeapps.list changed externally");
          return false;
        },
      400);
}

void
MainWindow::refresh_current_views ()
{
  if (!selected_app_id_.empty ())
    {
      show_app_detail (selected_app_id_);
    }
}

void
MainWindow::build_shell ()
{
  header_title_.set_text ("Default Apps");
  header_title_.add_css_class ("title");
  header_.set_title_widget (header_title_);
  set_titlebar (header_);

  root_.set_hexpand (true);
  root_.set_vexpand (true);
  set_child (root_);

  body_.set_hexpand (true);
  body_.set_vexpand (true);
  body_.set_spacing (0);
  root_.append (body_);

  nav_.set_stack (stack_);
  nav_.set_vexpand (true);
  nav_.set_size_request (150, -1);
  body_.append (nav_);

  stack_.set_hexpand (true);
  stack_.set_vexpand (true);
  stack_.set_transition_type (Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
  body_.append (stack_);

  status_label_.set_text ("Ready");
  status_label_.set_xalign (0.0f);
  status_label_.add_css_class ("dim-label");
  status_label_.set_margin_start (12);
  status_label_.set_margin_end (12);
  status_label_.set_margin_top (6);
  status_label_.set_margin_bottom (6);
  root_.append (status_label_);
}

void
MainWindow::build_apps_page ()
{
  stack_.add (apps_page_, "apps", "Apps");

  apps_paned_.set_hexpand (true);
  apps_paned_.set_vexpand (true);
  apps_paned_.set_position (300);
  apps_paned_.set_wide_handle (true);
  apps_page_.append (apps_paned_);

  // Left: search + app list (Windows: "Search apps" + app grid).
  apps_left_.set_spacing (8);
  apps_left_.set_margin_start (12);
  apps_left_.set_margin_end (6);
  apps_left_.set_margin_top (12);
  apps_left_.set_margin_bottom (12);
  apps_left_.set_size_request (280, -1);
  apps_paned_.set_start_child (apps_left_);

  apps_search_.set_placeholder_text ("Search apps");
  apps_search_.set_hexpand (true);
  apps_search_.signal_search_changed ().connect ([this] ()
                                                   { refresh_apps_list (); });
  apps_left_.append (apps_search_);

  apps_scroll_.set_vexpand (true);
  apps_scroll_.set_hexpand (true);
  apps_scroll_.set_policy (Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  apps_left_.append (apps_scroll_);

  apps_list_.set_selection_mode (Gtk::SelectionMode::SINGLE);
  apps_list_.set_show_separators (true);
  apps_scroll_.set_child (apps_list_);

  apps_list_.signal_row_selected ().connect (
      [this] (Gtk::ListBoxRow *row)
        {
          if (row == nullptr)
            {
              return;
            }
          int idx = row->get_index ();
          if (idx >= 0 && idx < static_cast<int> (filtered_apps_.size ()))
            {
              show_app_detail (filtered_apps_[static_cast<size_t> (idx)].id);
            }
        });

  // Right: detail for the selected app
  // (Windows: app header + "Set default" + per-type rows).
  detail_box_.set_hexpand (true);
  detail_box_.set_vexpand (true);
  detail_box_.set_spacing (8);
  detail_box_.set_margin_start (6);
  detail_box_.set_margin_end (12);
  detail_box_.set_margin_top (12);
  detail_box_.set_margin_bottom (12);
  apps_paned_.set_end_child (detail_box_);

  detail_scroll_.set_vexpand (true);
  detail_scroll_.set_hexpand (true);
  detail_scroll_.set_policy (Gtk::PolicyType::NEVER,
                             Gtk::PolicyType::AUTOMATIC);
  detail_box_.append (detail_scroll_);

  detail_content_.set_spacing (12);
  detail_content_.set_margin_start (8);
  detail_content_.set_margin_end (8);
  detail_content_.set_margin_top (8);
  detail_content_.set_margin_bottom (8);
  detail_scroll_.set_child (detail_content_);

  detail_header_.set_spacing (12);
  detail_content_.append (detail_header_);

  detail_icon_.set_pixel_size (48);
  detail_icon_.set_from_icon_name ("application-x-executable");
  detail_header_.append (detail_icon_);

  detail_titles_.set_hexpand (true);
  detail_titles_.set_vexpand (false);
  detail_titles_.set_spacing (2);
  detail_header_.append (detail_titles_);

  detail_name_.set_xalign (0.0f);
  detail_name_.add_css_class ("title-2");
  detail_titles_.append (detail_name_);

  detail_sub_.set_xalign (0.0f);
  detail_sub_.add_css_class ("dim-label");
  detail_sub_.set_wrap (true);
  detail_titles_.append (detail_sub_);

  set_default_button_.set_label ("Set default");
  set_default_button_.add_css_class ("suggested-action");
  set_default_button_.set_valign (Gtk::Align::CENTER);
  set_default_button_.signal_clicked ().connect (
      [this] ()
        {
          if (selected_app_id_.empty ())
            {
              return;
            }
          const AppEntry *found = nullptr;
          for (auto &a : apps_)
            {
              if (a.id == selected_app_id_)
                {
                  found = &a;
                  break;
                }
            }
          if (found == nullptr || !found->gapp)
            {
              return;
            }
          int ok_count = 0;
          int fail_count = 0;
          std::string err;
          for (auto &mime : found->supported_types)
            {
              if (service_.set_default (found->gapp, mime, err))
                {
                  ++ok_count;
                }
              else
                {
                  ++fail_count;
                }
            }
          if (fail_count == 0)
            {
              set_status (Glib::ustring ("Set ") + found->name
                          + " as default for " + std::to_string (ok_count)
                          + " types");
            }
          else
            {
              set_status (Glib::ustring ("Set ") + std::to_string (ok_count)
                          + " defaults, " + std::to_string (fail_count)
                          + " failed: " + err);
            }
          reload_all ();
          show_app_detail (selected_app_id_);
        });
  detail_header_.append (set_default_button_);

  detail_count_.set_xalign (0.0f);
  detail_count_.add_css_class ("dim-label");
  detail_content_.append (detail_count_);

  detail_types_list_.set_selection_mode (Gtk::SelectionMode::NONE);
  detail_types_list_.set_show_separators (true);
  detail_content_.append (detail_types_list_);
}

void
MainWindow::build_filetypes_page ()
{
  stack_.add (filetypes_page_, "types", "File types");

  filetypes_page_.set_spacing (8);
  filetypes_page_.set_margin_start (12);
  filetypes_page_.set_margin_end (12);
  filetypes_page_.set_margin_top (12);
  filetypes_page_.set_margin_bottom (12);

  auto *hint = Gtk::make_managed<Gtk::Label> (
      "Choose defaults by file type (MIME). Mirrors Windows “Defaults by file "
      "type”.");
  hint->set_xalign (0.0f);
  hint->add_css_class ("dim-label");
  hint->set_wrap (true);
  filetypes_page_.append (*hint);

  filetypes_search_.set_placeholder_text (
      "Search file types (.pdf, image/png, …)");
  filetypes_search_.signal_search_changed ().connect (
      [this] () { refresh_filetypes_list (); });
  filetypes_page_.append (filetypes_search_);

  filetypes_scroll_.set_vexpand (true);
  filetypes_scroll_.set_hexpand (true);
  filetypes_scroll_.set_policy (Gtk::PolicyType::NEVER,
                                Gtk::PolicyType::AUTOMATIC);
  filetypes_page_.append (filetypes_scroll_);

  filetypes_list_.set_selection_mode (Gtk::SelectionMode::NONE);
  filetypes_list_.set_show_separators (true);
  filetypes_scroll_.set_child (filetypes_list_);
}

void
MainWindow::build_schemes_page ()
{
  stack_.add (schemes_page_, "links", "Link types");

  schemes_page_.set_spacing (8);
  schemes_page_.set_margin_start (12);
  schemes_page_.set_margin_end (12);
  schemes_page_.set_margin_top (12);
  schemes_page_.set_margin_bottom (12);

  auto *hint = Gtk::make_managed<Gtk::Label> (
      "Choose defaults by link type (URL protocol, x-scheme-handler/*). "
      "Mirrors Windows “Defaults by link type”.");
  hint->set_xalign (0.0f);
  hint->add_css_class ("dim-label");
  hint->set_wrap (true);
  schemes_page_.append (*hint);

  schemes_search_.set_placeholder_text (
      "Search link types (http, https, mailto, …)");
  schemes_search_.signal_search_changed ().connect (
      [this] () { refresh_schemes_list (); });
  schemes_page_.append (schemes_search_);

  schemes_scroll_.set_vexpand (true);
  schemes_scroll_.set_hexpand (true);
  schemes_scroll_.set_policy (Gtk::PolicyType::NEVER,
                              Gtk::PolicyType::AUTOMATIC);
  schemes_page_.append (schemes_scroll_);

  schemes_list_.set_selection_mode (Gtk::SelectionMode::NONE);
  schemes_list_.set_show_separators (true);
  schemes_scroll_.set_child (schemes_list_);
}

void
MainWindow::reload_all ()
{
  spdlog::info ("Loading installed apps and MIME database...");
  apps_ = service_.load_apps ();
  file_types_ = service_.load_file_types ();
  schemes_ = service_.load_schemes ();
  spdlog::info ("Found {} apps, {} file types, {} link types", apps_.size (),
                file_types_.size (), schemes_.size ());

  refresh_apps_list ();
  refresh_filetypes_list ();
  refresh_schemes_list ();
  set_status (
      Glib::ustring (std::to_string (apps_.size ()) + " apps · "
                     + std::to_string (file_types_.size ()) + " file types · "
                     + std::to_string (schemes_.size ()) + " link types"));
}

auto
MainWindow::contains_ci (const std::string &haystack,
                         const Glib::ustring &needle) -> bool
{
  if (needle.empty ())
    {
      return true;
    }
  std::string n = needle.raw ();
  std::string h = haystack;
  std::transform (h.begin (), h.end (), h.begin (),
                  [] (unsigned char c) { return std::tolower (c); });
  std::transform (n.begin (), n.end (), n.begin (),
                  [] (unsigned char c) { return std::tolower (c); });
  return h.find (n) != std::string::npos;
}

void
MainWindow::refresh_apps_list ()
{
  apps_list_.remove_all ();
  filtered_apps_.clear ();

  Glib::ustring query = apps_search_.get_text ();
  for (auto &app : apps_)
    {
      std::string hay = std::string (app.name.raw ()) + " " + app.id + " "
                        + std::string (app.description.raw ());
      if (!contains_ci (hay, query))
        {
          continue;
        }
      filtered_apps_.push_back (app);
    }

  for (auto &app : filtered_apps_)
    {
      apps_list_.append (*make_app_row (app));
    }

  // Keep selection on the currently open app if it is still visible.
  for (size_t i = 0; i < filtered_apps_.size (); ++i)
    {
      if (filtered_apps_[i].id == selected_app_id_)
        {
          if (auto *row = apps_list_.get_row_at_index (static_cast<int> (i)))
            {
              apps_list_.select_row (*row);
            }
          break;
        }
    }
}

void
MainWindow::refresh_filetypes_list ()
{
  filetypes_list_.remove_all ();
  filtered_file_types_.clear ();

  Glib::ustring query = filetypes_search_.get_text ();
  size_t shown = 0;
  constexpr size_t kLimit = 800; // avoid building 10k rows at once
  for (auto &entry : file_types_)
    {
      std::string hay = entry.mime + " " + entry.description + " "
                        + entry.extensions_label;
      if (!contains_ci (hay, query))
        {
          continue;
        }
      filtered_file_types_.push_back (entry);
      if (++shown > kLimit && query.empty ())
        {
          break;
        }
    }
  for (auto &entry : filtered_file_types_)
    {
      filetypes_list_.append (*make_type_row (entry, true));
    }
  if (query.empty () && filtered_file_types_.size () < file_types_.size ())
    {
      auto *note = Gtk::make_managed<Gtk::Label> (
          Glib::ustring ("Showing ")
          + std::to_string (filtered_file_types_.size ()) + " of "
          + std::to_string (file_types_.size ())
          + " types — search to narrow down.");
      note->add_css_class ("dim-label");
      note->set_wrap (true);
      note->set_margin_start (8);
      note->set_margin_end (8);
      note->set_margin_top (8);
      note->set_margin_bottom (8);
      filetypes_list_.append (*note);
    }
}

void
MainWindow::refresh_schemes_list ()
{
  schemes_list_.remove_all ();
  filtered_schemes_.clear ();

  Glib::ustring query = schemes_search_.get_text ();
  for (auto &entry : schemes_)
    {
      std::string hay
          = entry.mime + " " + entry.scheme + " " + entry.description;
      if (!contains_ci (hay, query))
        {
          continue;
        }
      filtered_schemes_.push_back (entry);
      schemes_list_.append (*make_type_row (entry, true));
    }
}

void
MainWindow::show_app_detail (const std::string &app_id)
{
  selected_app_id_ = app_id;
  const AppEntry *found = nullptr;
  for (auto &a : apps_)
    {
      if (a.id == app_id)
        {
          found = &a;
          break;
        }
    }
  if (found == nullptr)
    {
      detail_name_.set_text ("No app selected");
      detail_sub_.set_text ("");
      detail_count_.set_text ("");
      detail_types_list_.remove_all ();
      set_default_button_.set_sensitive (false);
      return;
    }
  set_default_button_.set_sensitive (true);

  detail_name_.set_text (found->name);
  Glib::ustring sub = found->id;
  if (!found->description.empty ())
    {
      sub += Glib::ustring (" — ") + found->description;
    }
  detail_sub_.set_text (sub);

  if (found->gapp && found->gapp->get_icon ())
    {
      detail_icon_.set (found->gapp->get_icon ());
    }
  else
    {
      detail_icon_.set_from_icon_name ("application-x-executable");
    }

  // Split supported types like Windows does (file types vs link types).
  std::vector<std::string> sorted = found->supported_types;
  std::sort (sorted.begin (), sorted.end ());
  detail_mimes_ = sorted;

  size_t n_scheme = 0;
  for (auto &m : sorted)
    {
      if (MimeService::is_scheme_mime (m))
        {
          ++n_scheme;
        }
    }
  size_t n_file = sorted.size () > n_scheme ? sorted.size () - n_scheme : 0;
  detail_count_.set_text (
      Glib::ustring (std::to_string (n_file) + " file types · "
                     + std::to_string (n_scheme) + " link types"));

  detail_types_list_.remove_all ();
  if (sorted.empty ())
    {
      auto *label = Gtk::make_managed<Gtk::Label> (
          "This app does not declare any MIME types in its .desktop file.");
      label->add_css_class ("dim-label");
      label->set_wrap (true);
      detail_types_list_.append (*label);
      return;
    }
  for (auto &mime : sorted)
    {
      detail_types_list_.append (*make_detail_type_row (mime));
    }
}

auto
MainWindow::app_display_name (const Glib::RefPtr<Gio::AppInfo> &app,
                              const std::string &fallback) -> Glib::ustring
{
  if (app)
    {
      try
        {
          auto n = app->get_name ();
          if (!n.empty ())
            {
              return n;
            }
        }
      catch (...)
        {
        }
    }
  return Glib::ustring (fallback);
}

auto
MainWindow::default_label_for (const std::string &mime) -> Glib::ustring
{
  auto def = service_.default_for (mime);
  if (!def)
    {
      return "None";
    }
  return app_display_name (def, "Unknown");
}

auto
MainWindow::make_app_row (const AppEntry &app) -> Gtk::Widget *
{
  auto *box = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::HORIZONTAL, 12);
  box->set_margin_start (8);
  box->set_margin_end (8);
  box->set_margin_top (6);
  box->set_margin_bottom (6);

  auto *img = Gtk::make_managed<Gtk::Image> ();
  img->set_pixel_size (32);
  bool has_icon = false;
  if (app.gapp)
    {
      try
        {
          auto icon = app.gapp->get_icon ();
          if (icon)
            {
              img->set (icon);
              has_icon = true;
            }
        }
      catch (...)
        {
        }
    }
  if (!has_icon)
    {
      img->set_from_icon_name ("application-x-executable");
    }
  img->set_valign (Gtk::Align::CENTER);
  box->append (*img);

  auto *texts = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::VERTICAL, 2);
  texts->set_hexpand (true);
  box->append (*texts);

  auto *name = Gtk::make_managed<Gtk::Label> (app.name);
  name->set_xalign (0.0f);
  name->set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (*name);

  auto *sub = Gtk::make_managed<Gtk::Label> (app.id);
  sub->set_xalign (0.0f);
  sub->add_css_class ("dim-label");
  sub->set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (*sub);

  return box;
}

auto
MainWindow::make_type_row (const MimeTypeEntry &entry, bool show_change_button)
    -> Gtk::Widget *
{
  auto *box = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::HORIZONTAL, 12);
  box->set_margin_start (8);
  box->set_margin_end (8);
  box->set_margin_top (6);
  box->set_margin_bottom (6);

  auto *img = Gtk::make_managed<Gtk::Image> ();
  img->set_pixel_size (32);
  try
    {
      auto icon = Gio::content_type_get_icon (entry.mime);
      if (icon)
        {
          img->set (icon);
        }
      else
        {
          img->set_from_icon_name (entry.is_scheme ? "emblem-shared"
                                                   : "text-x-generic");
        }
    }
  catch (...)
    {
      img->set_from_icon_name (entry.is_scheme ? "emblem-shared"
                                               : "text-x-generic");
    }
  img->set_valign (Gtk::Align::CENTER);
  box->append (*img);

  auto *texts = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::VERTICAL, 2);
  texts->set_hexpand (true);
  box->append (*texts);

  Glib::ustring title = entry.description;
  auto *top = Gtk::make_managed<Gtk::Label> (title);
  top->set_xalign (0.0f);
  top->set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (*top);

  Glib::ustring bottom = entry.extensions_label;
  if (bottom != entry.mime)
    {
      bottom += Glib::ustring ("  ·  ") + entry.mime;
    }
  auto *sub = Gtk::make_managed<Gtk::Label> (bottom);
  sub->set_xalign (0.0f);
  sub->add_css_class ("dim-label");
  sub->set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (*sub);

  auto *def = Gtk::make_managed<Gtk::Label> (default_label_for (entry.mime));
  def->set_valign (Gtk::Align::CENTER);
  def->add_css_class ("dim-label");
  // XDG provenance: user override vs system default. Helps Linux users
  // understand where a default comes from and when Reset applies.
  {
    std::string src = service_.default_source (entry.mime);
    if (src == "user")
      {
        def->set_text (def->get_text () + "  • user");
      }
    else if (src == "system")
      {
        def->set_text (def->get_text () + "  • system");
      }
  }
  box->append (*def);

  if (show_change_button)
    {
      std::string mime = entry.mime;
      auto *btn = Gtk::make_managed<Gtk::Button> ("Change");
      btn->set_valign (Gtk::Align::CENTER);
      btn->signal_clicked ().connect ([this, mime] ()
                                        { open_chooser (mime); });
      box->append (*btn);
    }

  return box;
}

auto
MainWindow::make_detail_type_row (const std::string &mime) -> Gtk::Widget *
{
  MimeTypeEntry entry;
  entry.mime = mime;
  entry.is_scheme = MimeService::is_scheme_mime (mime);
  entry.scheme = MimeService::scheme_name (mime);
  entry.description = service_.describe (mime);
  entry.extensions_label = service_.extensions_for (mime);
  return make_type_row (entry, true);
}

void
MainWindow::open_chooser (const std::string &mime)
{
  auto candidates = service_.apps_for (mime);
  auto current = service_.default_for (mime);
  std::string current_id;
  if (current)
    {
      try
        {
          current_id = current->get_id ();
        }
      catch (...)
        {
        }
    }

  auto *dialog
      = new Gtk::Dialog (Glib::ustring ("Default for ") + mime, *this, true);
  dialog->set_default_size (520, 560);
  dialog->add_button ("Close", Gtk::ResponseType::CLOSE);
  dialog->signal_response ().connect (
      [dialog] (int)
        {
          dialog->set_visible (false);
          delete dialog;
        });

  auto *area = dialog->get_content_area ();
  area->set_spacing (8);
  area->set_margin_start (12);
  area->set_margin_end (12);
  area->set_margin_top (12);
  area->set_margin_bottom (12);

  Glib::ustring desc;
  try
    {
      desc = Gio::content_type_get_description (mime);
    }
  catch (...)
    {
      desc = mime;
    }
  auto *title = Gtk::make_managed<Gtk::Label> (
      Glib::ustring ("Choose default app for ") + mime
      + (desc.empty () || desc == mime ? Glib::ustring ("")
                                       : Glib::ustring (" (") + desc + ")"));
  title->set_wrap (true);
  title->set_xalign (0.0f);
  area->append (*title);

  {
    std::string src = service_.default_source (mime);
    Glib::ustring src_text
        = src == "user"     ? "User override (~/.config/mimeapps.list)"
          : src == "system" ? "System default (defaults.list)"
                            : "No default set";
    auto *src_row
        = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::HORIZONTAL, 8);
    auto *src_label = Gtk::make_managed<Gtk::Label> (src_text);
    src_label->set_hexpand (true);
    src_label->set_xalign (0.0f);
    src_label->add_css_class ("dim-label");
    src_row->append (*src_label);
    if (src == "user")
      {
        auto *reset_btn = Gtk::make_managed<Gtk::Button> ("Reset to system");
        reset_btn->signal_clicked ().connect (
            [this, mime, dialog] ()
              {
                std::string err;
                if (service_.reset_to_system (mime, err))
                  {
                    set_status (Glib::ustring ("Reset ") + mime
                                + " to system default");
                  }
                else
                  {
                    set_status (Glib::ustring ("Reset failed for ") + mime
                                + ": " + err);
                  }
                reload_all ();
                refresh_current_views ();
                dialog->set_visible (false);
                delete dialog;
              });
        src_row->append (*reset_btn);
      }
    area->append (*src_row);
  }

  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow> ();
  scroll->set_vexpand (true);
  scroll->set_hexpand (true);
  scroll->set_policy (Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  area->append (*scroll);

  auto *list = Gtk::make_managed<Gtk::ListBox> ();
  list->set_selection_mode (Gtk::SelectionMode::NONE);
  list->set_show_separators (true);
  scroll->set_child (*list);

  // Show capable apps first, then every other visible app as fallback.
  std::vector<Glib::RefPtr<Gio::AppInfo>> rows = candidates;
  if (rows.empty ())
    {
      for (auto &a : apps_)
        {
          if (a.gapp)
            {
              rows.push_back (a.gapp);
            }
        }
    }

  if (rows.empty ())
    {
      auto *empty
          = Gtk::make_managed<Gtk::Label> ("No applications available.");
      empty->add_css_class ("dim-label");
      list->append (*empty);
    }

  for (auto &app : rows)
    {
      if (!app)
        {
          continue;
        }
      std::string id;
      Glib::ustring name;
      try
        {
          id = app->get_id ();
        }
      catch (...)
        {
          continue;
        }
      name = app_display_name (app, id);
      bool is_current = (!current_id.empty () && id == current_id);

      auto *box
          = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::HORIZONTAL, 12);
      box->set_margin_start (8);
      box->set_margin_end (8);
      box->set_margin_top (6);
      box->set_margin_bottom (6);

      auto *img = Gtk::make_managed<Gtk::Image> ();
      img->set_pixel_size (32);
      try
        {
          auto icon = app->get_icon ();
          if (icon)
            {
              img->set (icon);
            }
          else
            {
              img->set_from_icon_name ("application-x-executable");
            }
        }
      catch (...)
        {
          img->set_from_icon_name ("application-x-executable");
        }
      box->append (*img);

      auto *texts
          = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::VERTICAL, 2);
      texts->set_hexpand (true);
      box->append (*texts);

      Glib::ustring label = name + (is_current ? "  ● Default" : "");
      auto *name_label = Gtk::make_managed<Gtk::Label> (label);
      name_label->set_xalign (0.0f);
      name_label->set_ellipsize (Pango::EllipsizeMode::END);
      texts->append (*name_label);

      auto *id_label = Gtk::make_managed<Gtk::Label> (id);
      id_label->set_xalign (0.0f);
      id_label->add_css_class ("dim-label");
      id_label->set_ellipsize (Pango::EllipsizeMode::END);
      texts->append (*id_label);

      auto *set_btn = Gtk::make_managed<Gtk::Button> (
          is_current ? "Current" : "Set default");
      set_btn->set_valign (Gtk::Align::CENTER);
      set_btn->set_sensitive (!is_current);
      // Capture RefPtr by value to keep the app alive until clicked.
      set_btn->signal_clicked ().connect (
          [this, app, mime, dialog, name] ()
            {
              std::string err;
              if (service_.set_default (app, mime, err))
                {
                  set_status (Glib::ustring ("Default for ") + mime + " → "
                              + name);
                }
              else
                {
                  set_status (Glib::ustring ("Failed to set default for ")
                              + mime + ": " + err);
                }
              reload_all ();
              if (!selected_app_id_.empty ())
                {
                  show_app_detail (selected_app_id_);
                }
              dialog->set_visible (false);
              delete dialog;
            });
      box->append (*set_btn);

      list->append (*box);
    }

  dialog->set_visible (true);
}

void
MainWindow::set_status (const Glib::ustring &text)
{
  status_label_.set_text (text);
  spdlog::info ("{}", std::string (text.raw ()));
}

} // namespace salt
