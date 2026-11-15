#pragma once

#include "glibmm/ustring.h"
#include <type_traits>
#include <string>
#include <gtkmm.h>
#include "gtkmm/widget.h"
#include <gtkmm/image.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/cssprovider.h>
#include <string>

/* Loads a pixbuf with the given size from the given file, returns null if unsuccessful */
Glib::RefPtr<Gdk::Pixbuf> load_icon_pixbuf_safe(std::string icon_path, int size);

/* Loads a CssProvider from the given path to the file, returns null if unsuccessful*/
Glib::RefPtr<Gtk::CssProvider> load_css_from_path(std::string path);

struct WfIconLoadOptions
{
    int user_scale = -1;
    bool invert    = false;
};

namespace IconProvider
{
void invert_pixbuf(Glib::RefPtr<Gdk::Pixbuf>& pbuff);
bool image_set_icon(Gtk::Image & image, Glib::ustring path);
void load_custom_icons(std::string section_name);
}

/* Helpers to set accessibility fields */
void mark_a11y_decorative(Gtk::Widget& widget);
void set_a11y_prop(Gtk::Widget& widget, Gtk::Accessible::Property prop, const char *value);
void set_a11y_prop(Gtk::Widget& widget, Gtk::Accessible::Property prop, const std::string& value);
void set_a11y_prop(Gtk::Widget& widget, Gtk::Accessible::Property prop, const Glib::ustring& value);
void set_a11y_prop(Gtk::Widget& widget, Gtk::Accessible::Property prop, int value);

template<typename T, typename = std::enable_if_t<std::is_same_v<T, bool>>>
void set_a11y_prop(Gtk::Widget& widget, Gtk::Accessible::Property prop, T value)
{
    Glib::Value<bool> val;
    val.init(Glib::Value<bool>::value_type());
    val.set(value);
    widget.update_property(prop, val);
}
