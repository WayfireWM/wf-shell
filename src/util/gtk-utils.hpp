#pragma once

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

void invert_pixbuf(Glib::RefPtr<Gdk::Pixbuf>& pbuff);

void image_set_icon(Gtk::Image *image, std::string path);
