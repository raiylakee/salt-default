#pragma once

#include <giomm/file.h>
#include <giomm/filemonitor.h>
#include <gtkmm.h>

#include <string>
#include <vector>

#include "mime_service.h"

namespace salt
{

class MainWindow : public Gtk::ApplicationWindow
{
public:
  MainWindow ();

private:
  // backend
  MimeService service_;
  std::vector<AppEntry> apps_;
  std::vector<MimeTypeEntry> file_types_;
  std::vector<MimeTypeEntry> schemes_;

  // filtered views (index matches ListBox row index)
  std::vector<AppEntry> filtered_apps_;
  std::vector<MimeTypeEntry> filtered_file_types_;
  std::vector<MimeTypeEntry> filtered_schemes_;
  std::vector<std::string> detail_mimes_;

  std::string selected_app_id_;

  // live-reload when mimeapps.list changes externally (gio mime, portals…)
  Glib::RefPtr<Gio::File> config_file_;
  Glib::RefPtr<Gio::File> config_dir_;
  Glib::RefPtr<Gio::FileMonitor> config_file_monitor_;
  Glib::RefPtr<Gio::FileMonitor> config_dir_monitor_;
  sigc::connection config_reload_timeout_;
  void start_config_watch ();
  void schedule_config_reload ();

  // shell
  Gtk::Box root_;
  Gtk::HeaderBar header_;
  Gtk::Label header_title_;
  Gtk::Box body_;
  Gtk::StackSidebar nav_;
  Gtk::Stack stack_;
  Gtk::Label status_label_;

  // apps page
  Gtk::Box apps_page_;
  Gtk::Paned apps_paned_;
  Gtk::Box apps_left_;
  Gtk::SearchEntry apps_search_;
  Gtk::ScrolledWindow apps_scroll_;
  Gtk::ListBox apps_list_;
  Gtk::Box detail_box_;
  Gtk::ScrolledWindow detail_scroll_;
  Gtk::Box detail_content_;
  Gtk::Box detail_header_;
  Gtk::Image detail_icon_;
  Gtk::Box detail_titles_;
  Gtk::Label detail_name_;
  Gtk::Label detail_sub_;
  Gtk::Button set_default_button_;
  Gtk::Label detail_count_;
  Gtk::ListBox detail_types_list_;

  // file types page
  Gtk::Box filetypes_page_;
  Gtk::SearchEntry filetypes_search_;
  Gtk::ScrolledWindow filetypes_scroll_;
  Gtk::ListBox filetypes_list_;

  // schemes page
  Gtk::Box schemes_page_;
  Gtk::SearchEntry schemes_search_;
  Gtk::ScrolledWindow schemes_scroll_;
  Gtk::ListBox schemes_list_;

  void build_shell ();
  void build_apps_page ();
  void build_filetypes_page ();
  void build_schemes_page ();
  void reload_all ();
  void refresh_current_views ();

  void refresh_apps_list ();
  void refresh_filetypes_list ();
  void refresh_schemes_list ();
  void show_app_detail (const std::string &app_id);

  auto app_display_name (const Glib::RefPtr<Gio::AppInfo> &app,
                         const std::string &fallback) -> Glib::ustring;
  auto default_label_for (const std::string &mime) -> Glib::ustring;

  auto make_app_row (const AppEntry &app) -> Gtk::Widget *;
  auto make_type_row (const MimeTypeEntry &entry, bool show_change_button)
      -> Gtk::Widget *;
  auto make_detail_type_row (const std::string &mime) -> Gtk::Widget *;

  void open_chooser (const std::string &mime);
  void set_status (const Glib::ustring &text);

  static auto contains_ci (const std::string &haystack,
                           const Glib::ustring &needle) -> bool;
};

} // namespace salt
