#include <giomm/desktopappinfo.h>
#include <gtkmm/button.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>

#include "dock.hpp"
#include "toplevel-icon.hpp"
#include "gtk-utils.hpp"
#include <iostream>
#include <cassert>

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

        return Icon{};
    }

    /* Second method: Just look up the built-in icon theme,
     * perhaps some icon can be found there */

    void set_image_from_icon(Gtk::Image& image, std::string app_id)
    {
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

enum WfToplevelState
{
    WF_TOPLEVEL_STATE_ACTIVATED = (1 << 0),
    WF_TOPLEVEL_STATE_MAXIMIZED = (1 << 1),
    WF_TOPLEVEL_STATE_MINIMIZED = (1 << 2),
};

class WfToplevelIcon
{
    zwlr_foreign_toplevel_handle_v1 *handle;
    wl_output *output;

    uint32_t state;

    Gtk::Button button;
    Gtk::Image image;

    public:
    WfToplevelIcon(zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output)
    {
        this->handle = handle;
        this->output = output;

        button.add(image);
        button.set_tooltip_text("none");
        button.show_all();

        button.signal_clicked().connect_notify(
            sigc::mem_fun(this, &WfToplevelIcon::on_clicked));

        auto dock = WfDockApp::get().dock_for_wl_output(output);
        assert(dock); // ToplevelIcon is created only for existing outputs
        dock->get_container().pack_end(button);
    }

    void on_clicked()
    {
        if (state & WF_TOPLEVEL_STATE_MINIMIZED)
            zwlr_foreign_toplevel_handle_v1_unset_minimized(handle);
        else
            zwlr_foreign_toplevel_handle_v1_set_minimized(handle);

        /* Dock might have been destroyed, and so we'll be destroyed shortly,
         * but we still don't want to crash */
        auto dock = WfDockApp::get().dock_for_wl_output(output);
        if (dock)
            dock->return_focus();

    }

    void set_app_id(std::string app_id)
    {
        IconProvider::set_image_from_icon(image, app_id);
    }

    void set_title(std::string title)
    {
        button.set_tooltip_text(title);
    }

    void set_state(uint32_t state)
    {
        this->state = state;
    }
};

namespace
{
    extern zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl;
}

class WfToplevel::impl
{
    zwlr_foreign_toplevel_handle_v1 *handle;
    std::map<wl_output*, std::unique_ptr<WfToplevelIcon>> icons;
    std::string _title, _app_id;
    uint32_t _state = 0;

    public:
    impl(zwlr_foreign_toplevel_handle_v1* handle)
    {
        this->handle = handle;
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

        auto icon = std::unique_ptr<WfToplevelIcon>(
            new WfToplevelIcon(handle, output));

        icon->set_title(_title);
        icon->set_app_id(_app_id);
        icon->set_state(_state);

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

    void set_state(uint32_t state)
    {
        _state = state;
        for (auto& icon : icons)
            icon.second->set_state(state);
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

/* wl_array_for_each isn't supported in C++, so we have to manually
 * get the data from wl_array, see:
 *
 * https://gitlab.freedesktop.org/wayland/wayland/issues/34 */
template<class T>
static void array_for_each(wl_array *array, std::function<void(T)> func)
{
    assert(array->size % sizeof(T) == 0); // do not use malformed arrays
    for (T* entry = (T*)array->data; (char*)entry < ((char*)array->data + array->size); entry++)
    {
        func(*entry);
    }
}

static void handle_toplevel_state(void *data, toplevel_t, wl_array *state)
{
    uint32_t flags = 0;
    array_for_each<uint32_t> (state, [&flags] (uint32_t st)
    {
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            flags |= WF_TOPLEVEL_STATE_ACTIVATED;
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
            flags |= WF_TOPLEVEL_STATE_MAXIMIZED;
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            flags |= WF_TOPLEVEL_STATE_MINIMIZED;
    });

    auto impl = static_cast<WfToplevel::impl*> (data);
    impl->set_state(flags);
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
