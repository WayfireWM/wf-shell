#include "gtk-utils.hpp"

#include <glibmm.h>
#include <gtkmm/icontheme.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdkcairo.h>
#include <iostream>

Glib::RefPtr<Gdk::Pixbuf> load_icon_pixbuf_safe(std::string icon_path, int size)
{
    try
    {
        auto pb = Gdk::Pixbuf::create_from_file(icon_path, size, size);
        return pb;
    }
    catch(Glib::FileError&)
    {
        std::cerr << "Error loading file: " << icon_path << std::endl;
        return {};
    }
    catch(Gdk::PixbufError&)
    {
        std::cerr << "Pixbuf error: " << icon_path << std::endl;
        return {};
    }
    catch(...)
    {
        std::cerr << "Failed to load: " << icon_path << std::endl;
        return {};
    }
}

Glib::RefPtr<Gtk::CssProvider> load_css_from_path(std::string path)
{
    try
    {
        auto css = Gtk::CssProvider::create();
        css->load_from_path(path);
        return css;
    }
    catch(Glib::Error& err)
    {
        std::cerr << "Failed to load CSS: " << err.what() << std::endl;
        return {};
    }
    catch(...)
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
    int w = pbuff->get_width();
    int h = pbuff->get_height();

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

void set_image_pixbuf(Gtk::Image &image, Glib::RefPtr<Gdk::Pixbuf> pixbuf, int scale)
{
    auto pbuff = pixbuf->gobj();
    auto cairo_surface = gdk_cairo_surface_create_from_pixbuf(pbuff, scale, NULL);

    gtk_image_set_from_surface(image.gobj(), cairo_surface);
    cairo_surface_destroy(cairo_surface);
}

void set_image_icon(Gtk::Image& image, std::string icon_name, int size,
                    const WfIconLoadOptions& options,
                    const Glib::RefPtr<Gtk::IconTheme>& icon_theme)
{
    int scale = ((options.user_scale == -1) ?
                 image.get_scale_factor() : options.user_scale);
    int scaled_size = size * scale;

    Glib::RefPtr<Gdk::Pixbuf> pbuff;

    if (!icon_theme->lookup_icon(icon_name, scaled_size))
    {
        if (Glib::file_test(icon_name, Glib::FILE_TEST_EXISTS))
            pbuff = load_icon_pixbuf_safe(icon_name, scaled_size);
    }
    else
    {
        pbuff = icon_theme->load_icon(icon_name, scaled_size)
            ->scale_simple(scaled_size, scaled_size, Gdk::INTERP_BILINEAR);
    }

    if (!pbuff)
    {
        std::cerr << "Failed to load icon \"" << icon_name << "\"" << std::endl;
        return;
    }

    if (options.invert)
        invert_pixbuf(pbuff);

    set_image_pixbuf(image, pbuff, scale);
}

/* Gio::DesktopAppInfo
 *
 * Usually knowing the app_id, we can get a desktop app info from Gio
 * The filename is either the app_id + ".desktop" or lower_app_id + ".desktop"
 */
Glib::RefPtr<Gio::DesktopAppInfo> get_desktop_app_info(std::string app_id)
{
    Glib::RefPtr<Gio::DesktopAppInfo> app_info;

    // search also on user defined .desktop files
    std::string home_dir = std::getenv("HOME");
    home_dir += "/.local/share/applications/";

    std::vector<std::string> prefixes = {
        home_dir,
        "",
        "/usr/share/applications/",
        "/usr/share/applications/kde/",
        "/usr/share/applications/org.kde.",
        "/usr/local/share/applications/",
        "/usr/local/share/applications/org.kde."
    };

    std::string app_id_lowercase = app_id;
    for (auto& c : app_id_lowercase)
        c = std::tolower(c);

    // if app id is org.name.appname take only the appname part
    std::string app_id_basename = app_id.substr(
        app_id.rfind(".")+1, app_id.size()
    );

    std::string app_id_basename_lowercase = app_id;
    for (auto& c : app_id_basename_lowercase)
        c = std::tolower(c);

    std::vector<std::string> app_id_variations = {
        app_id,
        app_id_lowercase,
        app_id_basename,
        app_id_basename_lowercase
    };

    std::vector<std::string> suffixes = {
        "",
        ".desktop"
    };

    for (auto& prefix : prefixes)
    {
        for (auto& id : app_id_variations)
        {
            for (auto& suffix : suffixes)
            {
                if (!app_info)
                {
                    app_info = Gio::DesktopAppInfo
                        ::create_from_filename(prefix + id + suffix);
                }
            }
        }
    }

    // Perform a search and select best match
    if (!app_info)
    {
        std::vector<std::string> app_id_variations;

        app_id_variations.push_back(app_id);

        // If appid has dashes add first component to search
        if (app_id.find('-') != std::string::npos)
        {
            std::istringstream stream(app_id);
            std::string token;
            std::getline(stream, token, '-');

            app_id_variations.push_back(token);
        }

        std::string desktop_file = "";

        for (auto token : app_id_variations)
        {
            gchar*** desktop_list = g_desktop_app_info_search(token.c_str());
            if (desktop_list != nullptr && desktop_list[0] != nullptr)
            {
                for (size_t i=0; desktop_list[0][i]; i++)
                {
                    if (desktop_file == "")
                    {
                        desktop_file = desktop_list[0][i];
                    }
                    else
                    {
                        auto tmp_info = Gio::DesktopAppInfo::create(desktop_list[0][i]);
                        auto startup_class = tmp_info->get_startup_wm_class();

                        if (
                            startup_class == app_id
                            ||
                            startup_class == app_id_lowercase
                        )
                        {
                            desktop_file = desktop_list[0][i];
                            break;
                        }
                    }
                }
                g_strfreev(desktop_list[0]);
            }
            g_free(desktop_list);

            if(desktop_file != "")
            {
                app_info = Gio::DesktopAppInfo::create(desktop_file);
                break;
            }
        }
    }

    // If app has dots try each component
    if (!app_info && app_id.find('.') != std::string::npos)
    {
        std::istringstream stream(app_id);
        std::string token;

        while (std::getline(stream, token, '.'))
        {
           app_info = Gio::DesktopAppInfo::create(token + ".desktop");

           if (app_info)
               break;
        }
    }

    if (app_info)
        return app_info;

    return {};
}

bool set_image_from_icon(Gtk::Image& image,
    std::string app_id_list, int size, int scale)
{
    std::string app_id;
    std::istringstream stream(app_id_list);

    bool found_icon = false;

    std::string icon_name = "unknown";

    // Wayfire sends a list of app-id's in space separated format, other
    // compositors send a single app-id, but in any case this works fine
    while (stream >> app_id)
    {
        std::string app_name = app_id.substr(
            app_id.rfind(".")+1, app_id.size()
        );

        // Try to load icon from the DesktopAppInfo
        auto app_info = get_desktop_app_info(app_id);

        if (app_info && app_info->get_icon())
            icon_name = app_info->get_icon()->to_string();

        // Try directly looking up the icon, if it exists
        if (icon_name == "unknown")
        {
            if (Gtk::IconTheme::get_default()->lookup_icon(app_id, 24))
                icon_name = app_id;
            else if (Gtk::IconTheme::get_default()->lookup_icon(app_name, 24))
                icon_name = app_name;
        }

        if (icon_name != "unknown")
        {
            found_icon = true;
            break;
        }
    }

    WfIconLoadOptions options;
    options.user_scale = scale;
    set_image_icon(image, icon_name, size, options);

    if (found_icon)
        return true;

    std::cout << "Failed to load icon for any of " << app_id_list << std::endl;
    return false;
}
