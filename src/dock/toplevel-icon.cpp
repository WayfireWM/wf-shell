#include <giomm/desktopappinfo.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>

#include <gdkmm/display.h>
#include <gdkmm/seat.h>
#include <gdk/wayland/gdkwayland.h>

#include "dock.hpp"
#include "toplevel.hpp"
#include "toplevel-icon.hpp"
#include "gtk-utils.hpp"
#include <iostream>
#include <sstream>
#include <cassert>
#include "wf-option-wrap.hpp"

namespace IconProvider
{
void set_image_from_icon(Gtk::Image& image,
    std::string app_id_list, int size, int scale);
}

class WfToplevelIcon::impl
{
    zwlr_foreign_toplevel_handle_v1 *handle;
    wl_output *output;

    uint32_t state;
    bool closing = false;

    Gtk::Button button;
    Gtk::Image image;
    std::string app_id;
    WfOption<int> icon_height{"dock/icon_height"};

  public:
    impl(zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output)
    {
        this->handle = handle;
        this->output = output;

        button.set_child(image);
        button.set_tooltip_text("none");
        button.add_css_class("flat");
        button.add_css_class("toplevel-icon");

        button.signal_clicked().connect(
            sigc::mem_fun(*this, &WfToplevelIcon::impl::on_clicked));

        auto dock = WfDockApp::get().dock_for_wl_output(output);
        assert(dock); // ToplevelIcon is created only for existing outputs
        dock->add_child(button);
    }

    void on_clicked()
    {
        if (closing)
        {
            return;
        }

        if (!(state & WF_TOPLEVEL_STATE_ACTIVATED))
        {
            auto gseat = Gdk::Display::get_default()->get_default_seat();
            auto seat  = gdk_wayland_seat_get_wl_seat(gseat->gobj());
            zwlr_foreign_toplevel_handle_v1_activate(handle, seat);
        } else
        {
            send_rectangle_hint();
            if (state & WF_TOPLEVEL_STATE_MINIMIZED)
            {
                zwlr_foreign_toplevel_handle_v1_unset_minimized(handle);
            } else
            {
                zwlr_foreign_toplevel_handle_v1_set_minimized(handle);
            }
        }
    }

    void set_app_id(std::string app_id)
    {
        if (closing)
        {
            return;
        }

        this->app_id = app_id;
        IconProvider::set_image_from_icon(image,
            app_id,
            icon_height,
            button.get_scale_factor());
    }

    void send_rectangle_hint()
    {
        if (closing)
        {
            return;
        }

        Gtk::Widget *widget = &this->button;

        int x = 0, y = 0;
        int width = image.get_allocated_width();
        int height = image.get_allocated_height();

        while (widget)
        {
            x += widget->get_allocation().get_x();
            y += widget->get_allocation().get_y();
            widget = widget->get_parent();
        }

        auto dock = WfDockApp::get().dock_for_wl_output(output);
        if (!dock)
        {
            return;
        }

        zwlr_foreign_toplevel_handle_v1_set_rectangle(handle,
            dock->get_wl_surface(), x, y, width, height);
    }

    void set_title(std::string title)
    {
        if (closing)
        {
            return;
        }

        button.set_tooltip_text(title);
    }

    void close()
    {
        button.add_css_class("closing");
        closing = true;
    }

    void set_state(uint32_t state)
    {
        if (closing)
        {
            return;
        }

        this->state = state;
        bool is_activated = this->state & WF_TOPLEVEL_STATE_ACTIVATED;
        bool is_min = state & WF_TOPLEVEL_STATE_MINIMIZED;
        bool is_max = state & WF_TOPLEVEL_STATE_MAXIMIZED;
        if (is_activated)
        {
            button.add_css_class("activated");
        } else
        {
            button.remove_css_class("activated");
        }

        if (is_min)
        {
            button.add_css_class("minimized");
        } else
        {
            button.remove_css_class("minimized");
        }

        if (is_max)
        {
            button.add_css_class("maximized");
        } else
        {
            button.remove_css_class("maximized");
        }
    }

    ~impl()
    {
        auto dock = WfDockApp::get().dock_for_wl_output(output);
        if (dock)
        {
            dock->rem_child(button);
        }
    }
};

WfToplevelIcon::WfToplevelIcon(zwlr_foreign_toplevel_handle_v1 *handle,
    wl_output *output) : pimpl(new impl(handle, output))
{}
WfToplevelIcon::~WfToplevelIcon() = default;

void WfToplevelIcon::set_title(std::string title)
{
    return pimpl->set_title(title);
}

void WfToplevelIcon::set_app_id(std::string app_id)
{
    return pimpl->set_app_id(app_id);
}

void WfToplevelIcon::set_state(uint32_t state)
{
    return pimpl->set_state(state);
}

void WfToplevelIcon::close()
{
    return pimpl->close();
}

/* Icon loading functions */
namespace IconProvider
{
using Icon = Glib::RefPtr<Gio::Icon>;

namespace
{
std::string tolower(std::string str)
{
    for (auto& c : str)
    {
        c = std::tolower(c);
    }

    return str;
}

std::map<std::string, std::string> custom_icons;
}

void load_custom_icons()
{
    static const std::string prefix = "icon_mapping_";
    auto section = WayfireShellApp::get().config.get_section("dock");

    for (auto option : section->get_registered_options())
    {
        if (option->get_name().compare(0, prefix.length(), prefix) != 0)
        {
            continue;
        }

        auto app_id = option->get_name().substr(prefix.length());
        custom_icons[app_id] = option->get_value_str();
    }
}

bool set_custom_icon(Gtk::Image& image, std::string app_id, int size, int scale)
{
    if (!custom_icons.count(app_id))
    {
        return false;
    }

    image_set_icon(&image, custom_icons[app_id]);
    return true;
}

/* Gio::DesktopAppInfo
 *
 * Usually knowing the app_id, we can get a desktop app info from Gio
 * The filename is either the app_id + ".desktop" or lower_app_id + ".desktop" */
Icon get_from_desktop_app_info(std::string app_id)
{
    Glib::RefPtr<Gio::DesktopAppInfo> app_info;

    std::vector<std::string> prefixes = {
        "",
        "/usr/share/applications/",
        "/usr/share/applications/kde/",
        "/usr/share/applications/org.kde.",
        "/usr/share/applications/org.gnome.",
        "/usr/local/share/applications/",
        "/usr/local/share/applications/org.kde.",
        "/usr/local/share/applications/org.gnome.",
    };

    std::vector<std::string> app_id_variations = {
        app_id,
        tolower(app_id),
        tolower(app_id),
    };
    // e.g. org.gnome.Evince.desktop
    app_id_variations[2][0] = std::toupper(app_id_variations[2][0]);

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

    if (!app_info)
    {
        // special treatment for snap apps
        std::string prefix = "/var/lib/snapd/desktop/applications/";
        auto& id = app_id_variations[1]; // seems to be lower case
        for (auto& suffix : suffixes)
        {
            app_info = Gio::DesktopAppInfo::create_from_filename(
                prefix + id + "_" + id + suffix);
            if (app_info)
            {
                break;
            }
        }
    }

    if (app_info) // success
    {
        return app_info->get_icon();
    }

    return Icon{};
}

void set_image_from_icon(Gtk::Image& image,
    std::string app_id_list, int size, int scale)
{
    std::string app_id;
    std::istringstream stream(app_id_list);

    bool found_icon = false;

    /* Wayfire sends a list of app-id's in space separated format, other compositors
     * send a single app-id, but in any case this works fine */
    auto display = image.get_display();
    while (stream >> app_id)
    {
        /* Try first method: custom icon file provided by the user */
        if (set_custom_icon(image, app_id, size, scale))
        {
            found_icon = true;
            break;
        }

        /* Then try to load the DesktopAppInfo */
        auto icon = get_from_desktop_app_info(app_id);
        std::string icon_name = "unknown";

        if (!icon)
        {
            /* Finally try directly looking up the icon, if it exists */
            if (Gtk::IconTheme::get_for_display(display)->lookup_icon(app_id, 24))
            {
                icon_name = app_id;
            }
        } else
        {
            icon_name = icon->to_string();
        }

        WfIconLoadOptions options;
        options.user_scale = scale;
        image_set_icon(&image, icon_name);

        /* finally found some icon */
        if (icon_name != "unknown")
        {
            found_icon = true;
            break;
        }
    }

    if (!found_icon)
    {
        std::cout << "Failed to load icon for any of " << app_id_list << std::endl;
    }
}
}
