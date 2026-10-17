#pragma once

#include <giomm/file.h>
#include <giomm/filemonitor.h>
#include <gtkmm.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "mime_service.h"

namespace salt
{

class MainWindow : public Gtk::ApplicationWindow
{
public:
  MainWindow ();

private:
  MimeService service_;
  std::vector<AppEntry> apps_;
  std::vector<MimeTypeEntry> file_types_;
  std::vector<MimeTypeEntry> schemes_;
  std::unordered_map<std::string, AppEntry> app_index_;
  std::unordered_map<std::string, MimeTypeEntry> type_index_;

  std::string selected_app_id_;

  // live-reload when mimeapps.list changes externally (gio mime, portals…)
  Glib::RefPtr<Gio::File> config_file_;
  Glib::RefPtr<Gio::File> config_dir_;
  Glib::RefPtr<Gio::FileMonitor> config_file_monitor_;
  Glib::RefPtr<Gio::FileMonitor> config_dir_monitor_;
  sigc::connection config_reload_timeout_;
  void start_config_watch ();
  void schedule_config_reload ();

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
  Gtk::ListView apps_view_;
  Glib::RefPtr<Gtk::StringList> apps_model_;
  Glib::RefPtr<Gtk::SingleSelection> apps_selection_;
  Gtk::Label apps_info_;
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
  Gtk::Label filetypes_info_;
  Gtk::ScrolledWindow filetypes_scroll_;
  Gtk::ListView filetypes_view_;
  Glib::RefPtr<Gtk::StringList> filetypes_model_;

  // schemes page
  Gtk::Box schemes_page_;
  Gtk::SearchEntry schemes_search_;
  Gtk::Label schemes_info_;
  Gtk::ScrolledWindow schemes_scroll_;
  Gtk::ListView schemes_view_;
  Glib::RefPtr<Gtk::StringList> schemes_model_;

  void build_shell ();
  void build_apps_page ();
  void build_filetypes_page ();
  void build_schemes_page ();
  void setup_list_factories ();
  void reload_all ();
  void refresh_current_views ();

  void refresh_apps_list ();
  void refresh_filetypes_list ();
  void refresh_schemes_list ();
  void show_app_detail (const std::string &app_id);

  auto default_label_for (const std::string &mime) -> Glib::ustring;
  // "Name • user" / "Name • system" provenance text for a type row.
  auto type_default_text (const std::string &mime) -> Glib::ustring;
  auto lookup_type (const std::string &mime) -> const MimeTypeEntry *;

  auto make_type_row (const MimeTypeEntry &entry, bool show_change_button)
      -> Gtk::Widget *;
  auto make_detail_type_row (const std::string &mime) -> Gtk::Widget *;
  auto make_chooser_app_row (const AppEntry &app, const std::string &mime,
                             bool is_current, bool is_removed,
                             Gtk::Dialog *dialog) -> Gtk::Widget *;

  void open_chooser (const std::string &mime);
  void close_chooser_and_reopen (Gtk::Dialog *dialog, const std::string &mime);
  void set_status (const Glib::ustring &text);

  static auto contains_ci (const std::string &haystack,
                           const Glib::ustring &needle) -> bool;
};

} // namespace salt
