#ifndef WF_GTK_UTILS
#define WF_GTK_UTILS

#include <gtkmm/image.h>
#include <string>

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

#endif /* end of include guard: WF_GTK_UTILS */
