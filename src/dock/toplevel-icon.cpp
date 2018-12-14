#include <giomm/desktopappinfo.h>
#include <gtkmm/button.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>

#include "dock.hpp"
#include "toplevel-icon.hpp"
#include "gtk-utils.hpp"
#include <iostream>

/* TODO: split : we have 2 classes toplevel-icon and toplevel */

namespace IconProvider
{
    using Icon = Glib::RefPtr<Gio::Icon>;

    namespace
    {
        std::string tolower(std::string str)
        {
            for (auto& c : str)
                c = std::tolower(c);
            return str;
        }

        std::string format_gnome_app(std::string str)
        {
            str = tolower(str);
            if (str.size())
                str[0] = std::toupper(str[0]);

            return str;
        }
    }

    /* First method: Gio::DesktopAppInfo
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
            "/usr/share/applications/org.gnome.",
        };

        std::vector<std::string> app_id_variations = {
            app_id,
            tolower(app_id),
            format_gnome_app(app_id),
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

        if (app_info) // success
            return app_info->get_icon();

        std::cout << "but not found" << std::endl;
        return Icon{};
    }

    /* Second method: Just look up the built-in icon theme,
     * perhaps some icon can be found there */

    void set_image_from_icon(Gtk::Image& image, std::string app_id)
    {
        std::cout << "app id is " << app_id << std::endl;
        auto icon = get_from_desktop_app_info(app_id);

        if (icon)
        {
            gtk_image_set_from_gicon(image.gobj(), icon->gobj(),
                GTK_ICON_SIZE_DIALOG);
            return;
        }

        image.set_from_icon_name(app_id, Gtk::ICON_SIZE_DIALOG);
    }
};

class WfToplevelIcon
{
    Gtk::Button button;
    Gtk::Image image;

    public:
    WfToplevelIcon(WfDock& dock)
    {
        button.add(image);
        button.set_tooltip_text("none");
        button.show_all();

        dock.get_container().pack_end(button);
    }

    void set_app_id(std::string app_id)
    {
        IconProvider::set_image_from_icon(image, app_id);
     //   set_image_icon(image, "nautilus", 100, {});
    }

    void set_title(std::string title)
    {
        button.set_tooltip_text(title);
    }
};

namespace
{
    extern zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl;
}

class WfToplevel::impl
{
    std::map<wl_output*, std::unique_ptr<WfToplevelIcon>> icons;
    std::string _title, _app_id;

    public:
    impl(zwlr_foreign_toplevel_handle_v1* handle)
    {
        zwlr_foreign_toplevel_handle_v1_add_listener(handle,
            &toplevel_handle_v1_impl, this);
    }

    void handle_output_enter(wl_output *output)
    {
        if (icons.count(output))
            return;

        auto dock = WfDockApp::get().dock_for_wl_output(output);

        /* This catches two edge cases:
         * 1. The dock on the given output simply was closed by the user
         *
         * 2. The wl_output has been bound multiple times - this happens because
         * gtk will bind each output once, and then we bind it second time. So
         * the compositor will actually send the output_enter/leave at least
         * twice, and the one time when we get it with the output resource bound
         * by gtk, we need to ignore the request */
        if (!dock)
            return;

        auto icon = std::unique_ptr<WfToplevelIcon>(new WfToplevelIcon(*dock));
        icon->set_title(_title);
        icon->set_app_id(_app_id);

        icons[output] = std::move(icon);
    }

    void handle_output_leave(wl_output *output)
    {
        icons.erase(output);
    }

    void set_title(std::string title)
    {
        _title = title;
        for (auto& icon : icons)
            icon.second->set_title(title);
    }

    void set_app_id(std::string app_id)
    {
        _app_id = app_id;
        for (auto& icon : icons)
            icon.second->set_app_id(app_id);
    }
};

WfToplevel::WfToplevel(zwlr_foreign_toplevel_handle_v1 *handle)
    :pimpl(new WfToplevel::impl(handle)) { }
WfToplevel::~WfToplevel() = default;

void WfToplevel::handle_output_leave(wl_output *output)
{
    pimpl->handle_output_leave(output);
}

using toplevel_t = zwlr_foreign_toplevel_handle_v1*;
static void handle_toplevel_title(void *data, toplevel_t, const char *title)
{
    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->set_title(title);
}

static void handle_toplevel_app_id(void *data, toplevel_t, const char *app_id)
{
    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->set_app_id(app_id);
}

static void handle_toplevel_output_enter(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->handle_output_enter(output);
}

static void handle_toplevel_output_leave(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->handle_output_leave(output);
}

static void handle_toplevel_state(void *data, toplevel_t, wl_array *state)
{
//    auto impl = static_cast<WfToplevel::impl*> (data);
}

static void handle_toplevel_done(void *data, toplevel_t)
{
//    auto impl = static_cast<WfToplevel::impl*> (data);
}

static void handle_toplevel_closed(void *data, toplevel_t handle)
{
    WfDockApp::get().handle_toplevel_closed(handle);
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
}

namespace
{
struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl = {
    .title        = handle_toplevel_title,
    .app_id       = handle_toplevel_app_id,
    .output_enter = handle_toplevel_output_enter,
    .output_leave = handle_toplevel_output_leave,
    .state        = handle_toplevel_state,
    .done         = handle_toplevel_done,
    .closed       = handle_toplevel_closed
};
}
