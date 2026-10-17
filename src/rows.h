#pragma once

#include <gtkmm.h>

#include <functional>
#include <string>

#include "mime_service.h"

namespace salt
{

// Row widget for the app list.
class AppRow : public Gtk::Box
{
public:
  AppRow ();

  void bind (const AppEntry &app);

private:
  Gtk::Image icon_;
  Gtk::Label name_;
  Gtk::Label id_;
};

// Row widget for the file-type / link-type lists.
class TypeRow : public Gtk::Box
{
public:
  using ChangeSlot = std::function<void (const std::string &mime)>;

  TypeRow ();

  void bind (const MimeTypeEntry &entry, const Glib::ustring &default_text,
             int removed_count, const ChangeSlot &on_change);

private:
  Gtk::Image icon_;
  Gtk::Label title_;
  Gtk::Label subtitle_;
  Gtk::Label default_label_;
  Gtk::Label badge_;
  Gtk::Button change_;

  std::string mime_;
  ChangeSlot on_change_;
};

} // namespace salt
