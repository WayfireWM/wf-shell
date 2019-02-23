#include <gtkmm/image.h>
#include <gtkmm/button.h>
#include <gtkmm/icontheme.h>
#include <giomm/desktopappinfo.h>

#include "toplevel.hpp"
#include "panel.hpp"
#include <cassert>

namespace
{
    extern zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl;
}

namespace IconProvider
{
    void set_image_from_icon(Gtk::Image& image,
        std::string app_id_list, int size, int scale);
}

class WayfireToplevel::impl
{
    zwlr_foreign_toplevel_handle_v1 *handle;
    uint32_t state;

    Gtk::Button button;
    Gtk::Image image;
    std::string app_id;

    public:
    impl(zwlr_foreign_toplevel_handle_v1 *handle, Gtk::Box& container)
    {
        this->handle = handle;
        zwlr_foreign_toplevel_handle_v1_add_listener(handle,
            &toplevel_handle_v1_impl, this);


        button.add(image);
        button.set_tooltip_text("none");
        button.get_style_context()->add_class("flat");
        button.show_all();

        button.signal_clicked().connect_notify(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_clicked));
        button.signal_size_allocate().connect_notify(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_allocation_changed));
        button.property_scale_factor().signal_changed()
            .connect(sigc::mem_fun(this, &WayfireToplevel::impl::on_scale_update));

        container.pack_end(button);
    }

    void on_clicked()
    {
        if (!(state & WF_TOPLEVEL_STATE_ACTIVATED))
        {
            zwlr_foreign_toplevel_handle_v1_activate(handle,
                WayfirePanelApp::get().get_display()->default_seat);
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

    void on_allocation_changed(Gtk::Allocation& alloc)
    {
        send_rectangle_hint();
    }

    void on_scale_update()
    {
        set_app_id(app_id);
    }

    void set_app_id(std::string app_id)
    {
        this->app_id = app_id;
        IconProvider::set_image_from_icon(image, app_id,
            72, button.get_scale_factor());

    }

    void send_rectangle_hint()
    {
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

        /* TODO: Bad: we'll need to figure out how to get the panel we're on,
         * perhaps we need also panel_for_window because we can find out our toplevel window
         * the same as the loop above (going to widget parent)
         *
        auto panel = WayfirePanelApp::get().panel_for_wl_output(output);
        if (!panel)
            return;

        zwlr_foreign_toplevel_handle_v1_set_rectangle(handle,
            panel->get_wl_surface(), x, y, width, height);
            */
    }

    void set_title(std::string title)
    {
        button.set_tooltip_text(title);
    }

    void set_state(uint32_t state)
    {
        this->state = state;
    }

    ~impl()
    {
        //auto panel = WayfirePanelApp::get().panel_for_wl_output(output);
        //if (panel)
        //    panel->rem_child(button);
    }


    void handle_output_enter(wl_output *output)
    {
        /* Nothing for now */
    }

    void handle_output_leave(wl_output *output)
    {
        /* Nothing for now */
    }
};


WayfireToplevel::WayfireToplevel(zwlr_foreign_toplevel_handle_v1 *handle, Gtk::Box& container)
    :pimpl(new WayfireToplevel::impl(handle, container)) { }
WayfireToplevel::~WayfireToplevel() = default;

void WayfireToplevel::handle_output_enter(wl_output *output) { return pimpl->handle_output_enter(output); }
void WayfireToplevel::handle_output_leave(wl_output *output) { return pimpl->handle_output_leave(output); }

using toplevel_t = zwlr_foreign_toplevel_handle_v1*;
static void handle_toplevel_title(void *data, toplevel_t, const char *title)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->set_title(title);
}

static void handle_toplevel_app_id(void *data, toplevel_t, const char *app_id)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->set_app_id(app_id);
}

static void handle_toplevel_output_enter(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->handle_output_enter(output);
}

static void handle_toplevel_output_leave(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
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

    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->set_state(flags);
}

static void handle_toplevel_done(void *data, toplevel_t)
{
//    auto impl = static_cast<WayfireToplevel::impl*> (data);
}

static void handle_toplevel_closed(void *data, toplevel_t handle)
{
    //WayfirePanelApp::get().handle_toplevel_closed(handle);
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

/* Icon loading functions */
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
            "/usr/local/share/applications/",
            "/usr/local/share/applications/org.kde.",
        };

        std::vector<std::string> app_id_variations = {
            app_id,
            tolower(app_id),
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

    void set_image_from_icon(Gtk::Image& image,
        std::string app_id_list, int size, int scale)
    {
        std::string app_id;
        std::istringstream stream(app_id_list);

        /* Wayfire sends a list of app-id's in space separated format, other compositors
         * send a single app-id, but in any case this works fine */
        while (stream >> app_id)
        {
            auto icon = get_from_desktop_app_info(app_id);
            std::string icon_name = "unknown";

            if (!icon)
            {
                /* Perhaps no desktop app info, but we might still be able to
                 * get an icon directly from the icon theme */
                if (Gtk::IconTheme::get_default()->lookup_icon(app_id, 24))
                    icon_name = app_id;
            } else
            {
                icon_name = icon->to_string();
            }

            WfIconLoadOptions options;
            options.user_scale = scale;
            set_image_icon(image, icon_name, size, options);

            /* finally found some icon */
            if (icon_name != "unknown")
                break;
        }
    }
};
