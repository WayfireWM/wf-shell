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
#include <cassert>
#include "wf-option-wrap.hpp"


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
        IconProvider::image_set_icon(image,
            app_id);
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
