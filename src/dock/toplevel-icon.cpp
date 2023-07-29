#include <giomm/desktopappinfo.h>
#include <gtkmm/button.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>

#include <gdkmm/display.h>
#include <gdkmm/seat.h>
#include <gdk/gdkwayland.h>

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

    Gtk::Button button;
    Gtk::Image image;
    std::string app_id;
    WfOption<int> icon_height{"dock/icon_height"};

    public:
    impl(zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output)
    {
        this->handle = handle;
        this->output = output;

        button.add(image);
        button.set_tooltip_text("none");
        button.get_style_context()->add_class("flat");
        button.show_all();

        button.signal_clicked().connect_notify(
            sigc::mem_fun(this, &WfToplevelIcon::impl::on_clicked));
        button.signal_size_allocate().connect_notify(
            sigc::mem_fun(this, &WfToplevelIcon::impl::on_allocation_changed));
        button.property_scale_factor().signal_changed()
            .connect(sigc::mem_fun(this, &WfToplevelIcon::impl::on_scale_update));

        auto dock = WfDockApp::get().dock_for_wl_output(output);
        assert(dock); // ToplevelIcon is created only for existing outputs
        dock->add_child(button);
    }

    void on_clicked()
    {
        if (!(state & WF_TOPLEVEL_STATE_ACTIVATED))
        {
            auto gseat = Gdk::Display::get_default()->get_default_seat();
            auto seat = gdk_wayland_seat_get_wl_seat(gseat->gobj());
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
        IconProvider::set_image_from_icon(image,
                                          app_id,
                                          icon_height,
                                          button.get_scale_factor());
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

        auto dock = WfDockApp::get().dock_for_wl_output(output);
        if (!dock)
            return;

        zwlr_foreign_toplevel_handle_v1_set_rectangle(handle,
            dock->get_wl_surface(), x, y, width, height);
    }

    void set_title(std::string title)
    {
        button.set_tooltip_text(title);
    }

    void set_state(uint32_t state)
    {
        bool was_activated = this->state & WF_TOPLEVEL_STATE_ACTIVATED;
        this->state = state;
        bool is_activated = this->state & WF_TOPLEVEL_STATE_ACTIVATED;

        if (!was_activated && is_activated) {
            this->button.get_style_context()->remove_class("flat");
        } else if (was_activated && !is_activated) {
            this->button.get_style_context()->add_class("flat");
        }
    }

    ~impl()
    {
        auto dock = WfDockApp::get().dock_for_wl_output(output);
        if (dock)
            dock->rem_child(button);
    }
};

WfToplevelIcon::WfToplevelIcon(zwlr_foreign_toplevel_handle_v1 *handle,
    wl_output *output) : pimpl(new impl(handle, output)) {}
WfToplevelIcon::~WfToplevelIcon() = default;

void WfToplevelIcon::set_title(std::string title) { return pimpl->set_title(title); }
void WfToplevelIcon::set_app_id(std::string app_id) { return pimpl->set_app_id(app_id); }
void WfToplevelIcon::set_state(uint32_t state) { return pimpl->set_state(state); }

/* Icon loading functions */
namespace IconProvider
{
    using Icon = Glib::RefPtr<Gio::Icon>;

    namespace
    {
        std::map<std::string, std::string> custom_icons;
    }

    void load_custom_icons()
    {
        static const std::string prefix = "icon_mapping_";
        auto section = WayfireShellApp::get().config.get_section("dock");

        for (auto option : section->get_registered_options())
        {
            if (option->get_name().compare(0, prefix.length(), prefix) != 0)
                continue;

            auto app_id = option->get_name().substr(prefix.length());
            custom_icons[app_id] = option->get_value_str();
        }
    }

    bool set_custom_icon(Gtk::Image& image, std::string app_id, int size, int scale)
    {
        if (!custom_icons.count(app_id))
            return false;

        auto pb = load_icon_pixbuf_safe(custom_icons[app_id], size * scale);
        if (!pb.get())
            return false;

        set_image_pixbuf(image, pb, scale);
        return true;
    }

    void set_image_from_icon(Gtk::Image& image,
        std::string app_id_list, int size, int scale)
    {
        std::string app_id;
        std::istringstream stream(app_id_list);

        bool found_icon = false;

        /* Wayfire sends a list of app-id's in space separated format, other compositors
         * send a single app-id, but in any case this works fine */
        while (stream >> app_id)
        {
            std::string app_name = app_id.substr(
                app_id.rfind(".")+1, app_id.size()
            );

            /* Try first method: custom icon file provided by the user */
            if (
                set_custom_icon(image, app_id, size, scale)
                ||
                set_custom_icon(image, app_name, size, scale)
            )
            {
                found_icon = true;
                break;
            }
        }

        if (!found_icon)
            ::set_image_from_icon(image, app_id_list, size, scale);
    }
};
