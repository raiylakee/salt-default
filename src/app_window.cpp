#include "app_window.h"

#include <algorithm>
#include <cctype>

#include <giomm/contenttype.h>

#include "log.h"
#include "rows.h"

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
  setup_list_factories ();

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
          // Watch the directory: GIO replaces mimeapps.list atomically.
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

  apps_info_.set_xalign (0.0f);
  apps_info_.add_css_class ("dim-label");
  apps_info_.set_wrap (true);
  apps_info_.set_visible (false);
  apps_left_.append (apps_info_);

  apps_scroll_.set_vexpand (true);
  apps_scroll_.set_hexpand (true);
  apps_scroll_.set_policy (Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  apps_left_.append (apps_scroll_);

  // Virtualized: row widgets are recycled while scrolling.
  apps_model_ = Gtk::StringList::create ();
  apps_selection_ = Gtk::SingleSelection::create (apps_model_);
  apps_selection_->set_autoselect (false);
  apps_selection_->set_can_unselect (true);
  apps_view_.set_model (apps_selection_);
  apps_view_.set_show_separators (true);
  apps_scroll_.set_child (apps_view_);

  apps_selection_->signal_selection_changed ().connect (
      [this] (guint, guint)
        {
          auto sel = apps_selection_->get_selected ();
          if (sel == G_MAXUINT || sel >= apps_model_->get_n_items ())
            {
              return;
            }
          auto id = apps_model_->get_string (sel).raw ();
          if (id != selected_app_id_)
            {
              show_app_detail (id);
            }
        });

  // Right: detail for the selected app.
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
          if (found == nullptr)
            {
              return;
            }
          int ok_count = 0;
          int fail_count = 0;
          std::string err;
          for (auto &mime : found->supported_types)
            {
              if (service_.set_default (*found, mime, err))
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

  filetypes_info_.set_xalign (0.0f);
  filetypes_info_.add_css_class ("dim-label");
  filetypes_info_.set_wrap (true);
  filetypes_page_.append (filetypes_info_);

  filetypes_scroll_.set_vexpand (true);
  filetypes_scroll_.set_hexpand (true);
  filetypes_scroll_.set_policy (Gtk::PolicyType::NEVER,
                                Gtk::PolicyType::AUTOMATIC);
  filetypes_page_.append (filetypes_scroll_);

  filetypes_model_ = Gtk::StringList::create ();
  filetypes_view_.set_model (Gtk::NoSelection::create (filetypes_model_));
  filetypes_view_.set_show_separators (true);
  filetypes_scroll_.set_child (filetypes_view_);
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

  schemes_info_.set_xalign (0.0f);
  schemes_info_.add_css_class ("dim-label");
  schemes_info_.set_wrap (true);
  schemes_page_.append (schemes_info_);

  schemes_scroll_.set_vexpand (true);
  schemes_scroll_.set_hexpand (true);
  schemes_scroll_.set_policy (Gtk::PolicyType::NEVER,
                              Gtk::PolicyType::AUTOMATIC);
  schemes_page_.append (schemes_scroll_);

  schemes_model_ = Gtk::StringList::create ();
  schemes_view_.set_model (Gtk::NoSelection::create (schemes_model_));
  schemes_view_.set_show_separators (true);
  schemes_scroll_.set_child (schemes_view_);
}

void
MainWindow::reload_all ()
{
  spdlog::info ("Loading installed apps and MIME database...");
  // mimeapps.list may have changed externally; drop the parsed cache first.
  service_.invalidate_mimeapps_cache ();
  apps_ = service_.load_apps ();
  file_types_ = service_.load_file_types ();
  schemes_ = service_.load_schemes ();
  spdlog::info ("Found {} apps, {} file types, {} link types", apps_.size (),
                file_types_.size (), schemes_.size ());

  // Key -> entry maps so the ListView bind step is a hash lookup, not a scan.
  app_index_.clear ();
  for (auto &a : apps_)
    {
      app_index_[a.id] = a;
    }
  type_index_.clear ();
  for (auto &t : file_types_)
    {
      type_index_[t.mime] = t;
    }
  for (auto &t : schemes_)
    {
      type_index_[t.mime] = t;
    }

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
  Glib::ustring query = apps_search_.get_text ();

  std::vector<Glib::ustring> keys;
  for (auto &app : apps_)
    {
      std::string hay = std::string (app.name.raw ()) + " " + app.id + " "
                        + std::string (app.description.raw ());
      if (!contains_ci (hay, query))
        {
          continue;
        }
      keys.emplace_back (app.id);
    }

  // splice() swaps the model contents without rebuilding row widgets.
  apps_model_->splice (0, apps_model_->get_n_items (), keys);

  if (keys.empty ())
    {
      apps_info_.set_text ("No apps match your search.");
      apps_info_.set_visible (true);
    }
  else
    {
      apps_info_.set_visible (false);
    }

  // Keep the open app highlighted if it survived the filter.
  if (!selected_app_id_.empty ())
    {
      for (guint i = 0; i < keys.size (); ++i)
        {
          if (keys[i].raw () == selected_app_id_)
            {
              apps_selection_->set_selected (i);
              break;
            }
        }
    }
}

void
MainWindow::refresh_filetypes_list ()
{
  Glib::ustring query = filetypes_search_.get_text ();
  constexpr size_t kLimit = 800; // keep the unfiltered list snappy

  std::vector<Glib::ustring> keys;
  for (auto &entry : file_types_)
    {
      std::string hay = entry.mime + " " + entry.description + " "
                        + entry.extensions_label;
      if (!contains_ci (hay, query))
        {
          continue;
        }
      keys.emplace_back (entry.mime);
      if (query.empty () && keys.size () >= kLimit)
        {
          break;
        }
    }

  filetypes_model_->splice (0, filetypes_model_->get_n_items (), keys);

  if (keys.empty ())
    {
      filetypes_info_.set_text ("No file types match your search.");
    }
  else if (keys.size () < file_types_.size ())
    {
      filetypes_info_.set_text (Glib::ustring ("Showing ")
                                + std::to_string (keys.size ()) + " of "
                                + std::to_string (file_types_.size ())
                                + " types — search to narrow down.");
    }
  else
    {
      filetypes_info_.set_text (Glib::ustring (std::to_string (keys.size ()))
                                + " file types");
    }
}

void
MainWindow::refresh_schemes_list ()
{
  Glib::ustring query = schemes_search_.get_text ();

  std::vector<Glib::ustring> keys;
  for (auto &entry : schemes_)
    {
      std::string hay
          = entry.mime + " " + entry.scheme + " " + entry.description;
      if (!contains_ci (hay, query))
        {
          continue;
        }
      keys.emplace_back (entry.mime);
    }

  schemes_model_->splice (0, schemes_model_->get_n_items (), keys);

  if (keys.empty ())
    {
      schemes_info_.set_text ("No link types match your search.");
    }
  else
    {
      schemes_info_.set_text (Glib::ustring (std::to_string (keys.size ()))
                              + " link types");
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

  if (found->icon)
    {
      detail_icon_.set (found->icon);
    }
  else
    {
      detail_icon_.set_from_icon_name ("application-x-executable");
    }

  // Split supported types like Windows does (file types vs link types).
  std::vector<std::string> sorted = found->supported_types;
  std::sort (sorted.begin (), sorted.end ());

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

  // Keep the list highlight in sync when the detail pane changes elsewhere.
  for (guint i = 0; i < apps_model_->get_n_items (); ++i)
    {
      if (apps_model_->get_string (i).raw () == app_id)
        {
          if (apps_selection_->get_selected () != i)
            {
              apps_selection_->set_selected (i);
            }
          break;
        }
    }
}

auto
MainWindow::default_label_for (const std::string &mime) -> Glib::ustring
{
  // The id may name an entry GIO cannot build here, so look it up in apps_.
  const std::string id = service_.default_app_id (mime);
  if (id.empty ())
    {
      return "None";
    }
  auto it = app_index_.find (id);
  return it != app_index_.end () ? it->second.name : Glib::ustring (id);
}

void
MainWindow::setup_list_factories ()
{
  // setup() builds the row structure once per row; bind() only fills values.
  auto app_factory = Gtk::SignalListItemFactory::create ();
  app_factory->signal_setup ().connect (
      [] (const Glib::RefPtr<Gtk::ListItem> &item)
        { item->set_child (*Gtk::make_managed<AppRow> ()); });
  app_factory->signal_bind ().connect (
      [this] (const Glib::RefPtr<Gtk::ListItem> &item)
        {
          auto *row = dynamic_cast<AppRow *> (item->get_child ());
          auto obj = std::dynamic_pointer_cast<Gtk::StringObject> (
              item->get_item ());
          if (row == nullptr || !obj)
            {
              return;
            }
          auto it = app_index_.find (obj->get_string ().raw ());
          if (it == app_index_.end ())
            {
              return;
            }
          row->bind (it->second);
        });
  apps_view_.set_factory (app_factory);

  // Both type pages share one row layout.
  auto make_type_factory = [this] ()
    {
      auto factory = Gtk::SignalListItemFactory::create ();
      factory->signal_setup ().connect (
          [] (const Glib::RefPtr<Gtk::ListItem> &item)
            { item->set_child (*Gtk::make_managed<TypeRow> ()); });
      factory->signal_bind ().connect (
          [this] (const Glib::RefPtr<Gtk::ListItem> &item)
            {
              auto *row = dynamic_cast<TypeRow *> (item->get_child ());
              auto obj = std::dynamic_pointer_cast<Gtk::StringObject> (
                  item->get_item ());
              if (row == nullptr || !obj)
                {
                  return;
                }
              std::string mime = obj->get_string ().raw ();
              const MimeTypeEntry *entry = lookup_type (mime);
              if (entry == nullptr)
                {
                  return;
                }
              row->bind (*entry, type_default_text (mime),
                         static_cast<int> (
                             service_.removed_associations_for (mime).size ()),
                         [this] (const std::string &m) { open_chooser (m); });
            });
      return factory;
    };
  filetypes_view_.set_factory (make_type_factory ());
  schemes_view_.set_factory (make_type_factory ());
}

auto
MainWindow::lookup_type (const std::string &mime) -> const MimeTypeEntry *
{
  auto it = type_index_.find (mime);
  if (it == type_index_.end ())
    {
      return nullptr;
    }
  return &it->second;
}

auto
MainWindow::type_default_text (const std::string &mime) -> Glib::ustring
{
  Glib::ustring text = default_label_for (mime);
  std::string src = service_.default_source (mime);
  if (src == "user")
    {
      text += "  • user";
    }
  else if (src == "system")
    {
      text += "  • system";
    }
  return text;
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
  // XDG provenance: user override vs system default.
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

  // XDG [Removed Associations]: badge so users notice hidden apps exist.
  {
    auto removed = service_.removed_associations_for (entry.mime);
    if (!removed.empty ())
      {
        auto *badge = Gtk::make_managed<Gtk::Label> (
            Glib::ustring (std::to_string (removed.size ())) + " removed");
        badge->set_valign (Gtk::Align::CENTER);
        badge->add_css_class ("dim-label");
        badge->add_css_class ("error");
        box->append (*badge);
      }
  }

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
MainWindow::close_chooser_and_reopen (Gtk::Dialog *dialog,
                                      const std::string &mime)
{
  // Destroying the dialog from one of its own child handlers would free the
  // button mid-callback, so defer to the next idle.
  Glib::signal_idle ().connect_once (
      [this, dialog, mime] ()
        {
          dialog->set_visible (false);
          delete dialog;
          open_chooser (mime);
        });
}

auto
MainWindow::make_chooser_app_row (const AppEntry &app, const std::string &mime,
                                  bool is_current, bool is_removed,
                                  Gtk::Dialog *dialog) -> Gtk::Widget *
{
  const std::string id = app.id;
  const Glib::ustring name = app.name.empty () ? Glib::ustring (id) : app.name;

  auto *box = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::HORIZONTAL, 12);
  box->set_margin_start (8);
  box->set_margin_end (8);
  box->set_margin_top (6);
  box->set_margin_bottom (6);

  auto *img = Gtk::make_managed<Gtk::Image> ();
  img->set_pixel_size (32);
  bool has_icon = false;
  if (app.icon)
    {
      try
        {
          img->set (app.icon);
          has_icon = true;
        }
      catch (...)
        {
        }
    }
  if (!has_icon)
    {
      img->set_from_icon_name ("application-x-executable");
    }
  box->append (*img);

  auto *texts = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::VERTICAL, 2);
  texts->set_hexpand (true);
  box->append (*texts);

  Glib::ustring label = name;
  if (is_current)
    {
      label += "  ● Default";
    }
  else if (is_removed)
    {
      label += "  ● Removed";
    }
  auto *name_label = Gtk::make_managed<Gtk::Label> (label);
  name_label->set_xalign (0.0f);
  name_label->set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (*name_label);

  auto *id_label = Gtk::make_managed<Gtk::Label> (id);
  id_label->set_xalign (0.0f);
  id_label->add_css_class ("dim-label");
  id_label->set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (*id_label);

  if (is_removed)
    {
      // Has a [Removed Associations] entry: offer to re-enable it.
      auto *allow_btn = Gtk::make_managed<Gtk::Button> ("Allow");
      allow_btn->set_valign (Gtk::Align::CENTER);
      allow_btn->signal_clicked ().connect (
          [this, id, mime, dialog] ()
            {
              std::string err;
              if (service_.restore_association (id, mime, err))
                {
                  set_status (Glib::ustring ("Re-enabled ") + id + " for "
                              + mime);
                }
              else
                {
                  set_status (Glib::ustring ("Could not re-enable ") + id
                              + ": " + err);
                }
              reload_all ();
              refresh_current_views ();
              close_chooser_and_reopen (dialog, mime);
            });
      box->append (*allow_btn);
      return box;
    }

  auto *set_btn = Gtk::make_managed<Gtk::Button> (is_current ? "Current"
                                                             : "Set default");
  set_btn->set_valign (Gtk::Align::CENTER);
  set_btn->set_sensitive (!is_current);
  // Capture by value: apps_ is rebuilt on reload, so a reference could dangle.
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
              set_status (Glib::ustring ("Failed to set default for ") + mime
                          + ": " + err);
            }
          reload_all ();
          refresh_current_views ();
          close_chooser_and_reopen (dialog, mime);
        });
  box->append (*set_btn);

  // XDG [Removed Associations]: disallow this app for the type, keeping the
  // current default insensitive so the type cannot end up with none.
  auto *remove_btn = Gtk::make_managed<Gtk::Button> ();
  remove_btn->set_icon_name ("list-remove-symbolic");
  remove_btn->set_tooltip_text ("Remove this application for this type");
  remove_btn->add_css_class ("flat");
  remove_btn->set_valign (Gtk::Align::CENTER);
  remove_btn->set_sensitive (!is_current);
  remove_btn->signal_clicked ().connect (
      [this, id, mime, dialog] ()
        {
          std::string err;
          if (service_.remove_association (id, mime, err))
            {
              set_status (Glib::ustring ("Removed ") + id + " from " + mime);
            }
          else
            {
              set_status (Glib::ustring ("Could not remove ") + id + ": "
                          + err);
            }
          reload_all ();
          refresh_current_views ();
          close_chooser_and_reopen (dialog, mime);
        });
  box->append (*remove_btn);

  return box;
}

void
MainWindow::open_chooser (const std::string &mime)
{
  auto candidates = service_.apps_for_chooser (mime);
  // default_app_id() also covers the entries GIO cannot build here.
  const std::string current_id = service_.default_app_id (mime);
  auto removed_ids = service_.removed_associations_for (mime);
  auto is_removed_id = [&removed_ids] (const std::string &id)
    {
      return std::find (removed_ids.begin (), removed_ids.end (), id)
             != removed_ids.end ();
    };

  auto *dialog
      = new Gtk::Dialog (Glib::ustring ("Default for ") + mime, *this, true);
  dialog->set_default_size (560, 600);
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
                close_chooser_and_reopen (dialog, mime);
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

  // GIO hides removed apps from get_all_for_type(); filter them here.
  std::vector<AppEntry> rows;
  for (auto &app : candidates)
    {
      if (!app.id.empty ())
        {
          rows.push_back (app);
        }
    }
  if (rows.empty ())
    {
      for (auto &a : apps_)
        {
          if (!is_removed_id (a.id))
            {
              rows.push_back (a);
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
      const bool is_current = (!current_id.empty () && app.id == current_id);
      if (auto *row
          = make_chooser_app_row (app, mime, is_current, false, dialog))
        {
          list->append (*row);
        }
    }

  // Removed apps: GIO excludes them above, so surface them with "Allow".
  if (!removed_ids.empty ())
    {
      std::vector<AppEntry> removed_rows;
      for (auto &a : apps_)
        {
          if (is_removed_id (a.id))
            {
              removed_rows.push_back (a);
            }
        }
      if (!removed_rows.empty ())
        {
          auto *sep = Gtk::make_managed<Gtk::Label> ("Removed applications");
          sep->set_xalign (0.0f);
          sep->add_css_class ("dim-label");
          sep->set_margin_start (8);
          sep->set_margin_top (10);
          list->append (*sep);

          for (auto &app : removed_rows)
            {
              if (auto *row
                  = make_chooser_app_row (app, mime, false, true, dialog))
                {
                  list->append (*row);
                }
            }
        }
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
