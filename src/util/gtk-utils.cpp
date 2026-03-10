#include "glibmm/markup.h"
#include <gtk-utils.hpp>
#include <glibmm.h>
#include <gtkmm/icontheme.h>
#include <gdk/gdkcairo.h>
#include <iostream>

Glib::RefPtr<Gdk::Pixbuf> load_icon_pixbuf_safe(std::string icon_path, int size)
{
    try {
        auto pb = Gdk::Pixbuf::create_from_file(icon_path, size, size);
        return pb;
    } catch (Glib::FileError&)
    {
        std::cerr << "Error loading file: " << icon_path << std::endl;
        return {};
    } catch (Gdk::PixbufError&)
    {
        std::cerr << "Pixbuf error: " << icon_path << std::endl;
        return {};
    } catch (...)
    {
        std::cerr << "Failed to load: " << icon_path << std::endl;
        return {};
    }
}

Glib::RefPtr<Gtk::CssProvider> load_css_from_path(std::string path)
{
    try {
        auto css = Gtk::CssProvider::create();
        css->load_from_path(path);
        return css;
    } catch (Glib::Error& err)
    {
        std::cerr << "Failed to load CSS: " << err.what() << std::endl;
        return {};
    } catch (...)
    {
        std::cerr << "Failed to load CSS at: " << path << std::endl;
        return {};
    }
}

void invert_pixbuf(Glib::RefPtr<Gdk::Pixbuf>& pbuff)
{
    int channels = pbuff->get_n_channels();
    int stride   = pbuff->get_rowstride();

    auto data = pbuff->get_pixels();
    int w     = pbuff->get_width();
    int h     = pbuff->get_height();

    for (int i = 0; i < w; i++)
    {
        for (int j = 0; j < h; j++)
        {
            auto p = data + j * stride + i * channels;
            p[0] = 255 - p[0];
            p[1] = 255 - p[1];
            p[2] = 255 - p[2];
        }
    }
}

void image_set_icon(Gtk::Image *image, std::string path)
{
    if ((path.rfind("/", 0) == 0) || (path.rfind("~", 0) == 0))
    {
        image->set(path);
    } else
    {
        image->set_from_icon_name(path);
    }
}

/*
 * Check if this string appears to be markup
 *
 * Does not check it is *valid* markup
 */
bool is_markup(std::string input)
{
    int count_left  = std::count(input.begin(), input.end(), '<');
    int count_right = std::count(input.begin(), input.end(), '>');
    int count_amp   = std::count(input.begin(), input.end(), '&');
    int count_semi  = std::count(input.begin(), input.end(), ';');

    if (count_left || count_right || count_amp || count_semi)
    {
        /* It has some markup characters */
        if ((count_left == count_right) && (count_amp == count_semi))
        {
            /* And they pair up */
            return true;
        }
    }

    return false;
}

/* Escape string if it doesn't appear to be markup */
std::string markup_escape(std::string input)
{
    if (is_markup(input))
    {
        return input;
    }

    return Glib::Markup::escape_text(input);
}
