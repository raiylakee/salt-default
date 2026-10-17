#include "rows.h"

#include <giomm/contenttype.h>

#include <string>

namespace salt
{

AppRow::AppRow () : Gtk::Box (Gtk::Orientation::HORIZONTAL, 12)
{
  set_margin_start (8);
  set_margin_end (8);
  set_margin_top (6);
  set_margin_bottom (6);

  icon_ = Gtk::Image ();
  icon_.set_pixel_size (32);
  icon_.set_valign (Gtk::Align::CENTER);
  append (icon_);

  auto *texts = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::VERTICAL, 2);
  texts->set_hexpand (true);
  append (*texts);

  name_.set_xalign (0.0f);
  name_.set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (name_);

  id_.set_xalign (0.0f);
  id_.add_css_class ("dim-label");
  id_.set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (id_);
}

void
AppRow::bind (const AppEntry &app)
{
  name_.set_text (app.name);
  id_.set_text (app.id);

  bool has_icon = false;
  if (app.icon)
    {
      try
        {
          icon_.set (app.icon);
          has_icon = true;
        }
      catch (...)
        {
        }
    }
  if (!has_icon)
    {
      icon_.set_from_icon_name ("application-x-executable");
    }
}

TypeRow::TypeRow () : Gtk::Box (Gtk::Orientation::HORIZONTAL, 12)
{
  set_margin_start (8);
  set_margin_end (8);
  set_margin_top (6);
  set_margin_bottom (6);

  icon_ = Gtk::Image ();
  icon_.set_pixel_size (32);
  icon_.set_valign (Gtk::Align::CENTER);
  append (icon_);

  auto *texts = Gtk::make_managed<Gtk::Box> (Gtk::Orientation::VERTICAL, 2);
  texts->set_hexpand (true);
  append (*texts);

  title_.set_xalign (0.0f);
  title_.set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (title_);

  subtitle_.set_xalign (0.0f);
  subtitle_.add_css_class ("dim-label");
  subtitle_.set_ellipsize (Pango::EllipsizeMode::END);
  texts->append (subtitle_);

  default_label_.set_valign (Gtk::Align::CENTER);
  default_label_.add_css_class ("dim-label");
  append (default_label_);

  badge_.set_valign (Gtk::Align::CENTER);
  badge_.add_css_class ("dim-label");
  badge_.add_css_class ("error");
  badge_.set_visible (false);
  append (badge_);

  change_.set_label ("Change");
  change_.set_valign (Gtk::Align::CENTER);
  append (change_);

  // The row is recycled, so the handler reads the currently bound mime/slot.
  change_.signal_clicked ().connect (
      [this] ()
        {
          if (on_change_ && !mime_.empty ())
            {
              on_change_ (mime_);
            }
        });
}

void
TypeRow::bind (const MimeTypeEntry &entry, const Glib::ustring &default_text,
               int removed_count, const ChangeSlot &on_change)
{
  mime_ = entry.mime;
  on_change_ = on_change;

  bool has_icon = false;
  try
    {
      auto gicon = Gio::content_type_get_icon (entry.mime);
      if (gicon)
        {
          icon_.set (gicon);
          has_icon = true;
        }
    }
  catch (...)
    {
    }
  if (!has_icon)
    {
      icon_.set_from_icon_name (entry.is_scheme ? "emblem-shared"
                                                : "text-x-generic");
    }

  title_.set_text (entry.description);

  Glib::ustring sub = entry.extensions_label;
  if (sub != entry.mime)
    {
      sub += Glib::ustring ("  ·  ") + entry.mime;
    }
  subtitle_.set_text (sub);

  default_label_.set_text (default_text);

  if (removed_count > 0)
    {
      badge_.set_text (Glib::ustring (std::to_string (removed_count))
                       + " removed");
      badge_.set_visible (true);
    }
  else
    {
      badge_.set_visible (false);
    }
}

} // namespace salt
