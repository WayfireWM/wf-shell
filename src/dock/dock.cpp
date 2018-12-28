#include <gtkmm/window.h>
#include <glibmm/main.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>
#include <animation.hpp>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>
#include "dock.hpp"

namespace {
    extern zwlr_layer_surface_v1_listener layer_surface_v1_impl;
}

class WfDock::impl
{
    WayfireOutput *output;
    zwlr_layer_surface_v1 *layer_surface;
    wl_surface *surface;

    Gtk::Window window;
    Gtk::HBox box;

    wf_duration autohide_duration{new_static_option("300")};
    int get_hidden_margin()
    {
        static const int hidden_height = 1;
        return -(last_height - hidden_height);
    }

    void update_margin()
    {
        if (!autohide_duration.running())
            return;

        int margin = std::round(autohide_duration.progress());
        zwlr_layer_surface_v1_set_margin(layer_surface,
            margin, margin, margin, margin);
        wl_surface_commit(surface);
        window.queue_draw();
    }

    sigc::connection pending_hide;
    void do_show()
    {
        pending_hide.disconnect();
        autohide_duration.start(autohide_duration.progress(), 0);
        update_margin();
    }

    bool do_hide()
    {
        autohide_duration.start(autohide_duration.progress(),
            get_hidden_margin());
        update_margin();
        return false; // disconnect
    }

    void schedule_hide(int delay)
    {
        if (pending_hide.connected())
            return;
        pending_hide = Glib::signal_timeout().connect(
            sigc::mem_fun(this, &WfDock::impl::do_hide), delay);
    }

    void on_draw(const Cairo::RefPtr<Cairo::Context>& ctx)
    {
        update_margin();
    }

    void on_enter(GdkEventCrossing *cross)
    {
        // ignore events between the window and widgets
        if (cross->detail != GDK_NOTIFY_NONLINEAR &&
            cross->detail != GDK_NOTIFY_NONLINEAR_VIRTUAL)
            return;

        do_show();
    }

    void on_leave(GdkEventCrossing *cross)
    {
        // ignore events between the window and widgets
        if (cross->detail != GDK_NOTIFY_NONLINEAR &&
            cross->detail != GDK_NOTIFY_NONLINEAR_VIRTUAL)
            return;

        schedule_hide(500);
    }

    public:
    impl(WayfireOutput *output)
    {
        this->output = output;
        window.set_decorated(false);
        create_layer_surface();

        autohide_duration.start(get_hidden_margin(), get_hidden_margin());
        do_show();
        schedule_hide(500);

        window.add(box);

        window.signal_size_allocate().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_allocation));
        window.signal_enter_notify_event().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_enter));
        window.signal_leave_notify_event().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_leave));
        window.signal_draw().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_draw));
    }

    Gtk::HBox& get_container()
    {
        return box;
    }

    wl_surface* get_wl_surface()
    {
        return this->surface;
    }

    int32_t last_width = 100, last_height = 100;
    void on_allocation(Gtk::Allocation& alloc)
    {
        if (last_width != alloc.get_width() || last_height != alloc.get_height())
        {
            last_width = alloc.get_width();
            last_height = alloc.get_height();
            send_layer_surface_configure(last_width, last_height);
        }
    }

    void create_layer_surface()
    {
        auto gtk_window = window.gobj();
        auto gtk_widget = GTK_WIDGET(gtk_window);
        gtk_widget_realize(gtk_widget);

        auto gdk_window = window.get_window()->gobj();
        gdk_wayland_window_set_use_custom_surface(gdk_window);
        this->surface = gdk_wayland_window_get_wl_surface(gdk_window);

        layer_surface = zwlr_layer_shell_v1_get_layer_surface(
            output->display->zwlr_layer_shell, surface, output->handle,
            ZWLR_LAYER_SHELL_V1_LAYER_TOP, "wf-dock");

        zwlr_layer_surface_v1_add_listener(layer_surface,
            &layer_surface_v1_impl, this);

        zwlr_layer_surface_v1_set_anchor(layer_surface,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);

        zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface, 0);
        send_layer_surface_configure(last_width, last_height);
    }

    void send_layer_surface_configure(int width, int height)
    {
        zwlr_layer_surface_v1_set_size(layer_surface, width, height);
        wl_surface_commit(surface);
    }

    void handle_resized(int width, int height)
    {
        window.set_size_request(width, height);
        window.resize(width, height);
        window.show_all();

        zwlr_layer_surface_v1_set_size(layer_surface, width, height);
    }

    void return_focus() { }
};

static void handle_layer_surface_configure(void *data,
    zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
    uint32_t width, uint32_t height)
{
    auto impl = static_cast<WfDock::impl*> (data);

    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    impl->handle_resized(width, height);
}

static void handle_layer_surface_closed(void *data, zwlr_layer_surface_v1 *)
{
    // TODO
}

namespace {
    struct zwlr_layer_surface_v1_listener layer_surface_v1_impl = {
        .configure = handle_layer_surface_configure,
        .closed = handle_layer_surface_closed,
    };
}

WfDock::WfDock(WayfireOutput *output)
    : pimpl(new impl(output)) { }
WfDock::~WfDock() = default;

Gtk::HBox& WfDock::get_container() { return pimpl->get_container(); }
wl_surface* WfDock::get_wl_surface() { return pimpl->get_wl_surface(); }
