#ifndef WF_GTK_UTILS
#define WF_GTK_UTILS

#include <giomm/desktopappinfo.h>
#include <gtkmm/image.h>
#include <gtkmm/cssprovider.h>
#include <giomm/desktopappinfo.h>
#include <string>

/* Loads a pixbuf with the given size from the given file, returns null if unsuccessful */
Glib::RefPtr<Gdk::Pixbuf> load_icon_pixbuf_safe(std::string icon_path, int size);

/* Loads a CssProvider from the given path to the file, returns null if unsuccessful*/
Glib::RefPtr<Gtk::CssProvider> load_css_from_path(std::string path);

struct WfIconLoadOptions
{
    int user_scale = -1;
    bool invert = false;
};

/* Sets the content of the image to the pixbuf, applying device scale factor "scale" */
void set_image_pixbuf(Gtk::Image &image, Glib::RefPtr<Gdk::Pixbuf> pixbuf, int scale);

/* Sets the content of the image to the corresponding icon from the default theme,
 * using the given options */
void set_image_icon(Gtk::Image& image, std::string icon_name, int size,
                    const WfIconLoadOptions& options);

void invert_pixbuf(Glib::RefPtr<Gdk::Pixbuf>& pbuff);

Glib::RefPtr<Gio::DesktopAppInfo> get_desktop_app_info(std::string app_id);

bool set_image_from_icon(Gtk::Image& image,
                         std::string app_id_list, int size, int scale);

#endif /* end of include guard: WF_GTK_UTILS */
