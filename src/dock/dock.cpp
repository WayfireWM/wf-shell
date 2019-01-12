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

class WfDock::impl
{
    WayfireOutput *output;
    zwf_wm_surface_v1 *wm_surface;
    wl_surface *_wl_surface;

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
        zwf_wm_surface_v1_set_margin(wm_surface, margin, margin, margin, margin);
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
        window.add(box);
        window.set_size_request(100, 100);

        create_wm_surface();

        autohide_duration.start(get_hidden_margin(), get_hidden_margin());
        do_show();
        schedule_hide(500);

        window.signal_size_allocate().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_allocation));
        window.signal_enter_notify_event().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_enter));
        window.signal_leave_notify_event().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_leave));
        window.signal_draw().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_draw));
    }

    void add_child(Gtk::Widget& widget)
    {
        box.pack_end(widget);
    }

    void rem_child(Gtk::Widget& widget)
    {
        this->box.remove(widget);

        /* We now need to resize the dock so it fits the remaining widgets. */
        int total_width = 0;
        int total_height = last_height;
        box.foreach([&] (Gtk::Widget& child)
        {
            Gtk::Requisition min_req, pref_req;
            child.get_preferred_size(min_req, pref_req);

            total_width += min_req.width;
            total_height = std::max(total_height, min_req.height);
        });

        total_width = std::min(total_height, 100);
        this->window.resize(total_width, total_height);
        this->window.set_size_request(total_width, total_height);
    }

    wl_surface* get_wl_surface()
    {
        return this->_wl_surface;
    }

    int32_t last_width = 100, last_height = 100;
    void on_allocation(Gtk::Allocation& alloc)
    {
        if (last_width != alloc.get_width() || last_height != alloc.get_height())
        {
            last_width = alloc.get_width();
            last_height = alloc.get_height();
        }
    }

    void create_wm_surface()
    {
        window.show_all();

        auto gdk_window = window.get_window()->gobj();
        _wl_surface = gdk_wayland_window_get_wl_surface(gdk_window);

        wm_surface = zwf_output_v1_get_wm_surface(output->zwf,
            _wl_surface, ZWF_OUTPUT_V1_WM_ROLE_PANEL);

        zwf_wm_surface_v1_set_anchor(wm_surface,
            ZWF_WM_SURFACE_V1_ANCHOR_EDGE_BOTTOM);
        update_margin();

        zwf_wm_surface_v1_set_keyboard_mode(wm_surface,
            ZWF_WM_SURFACE_V1_KEYBOARD_FOCUS_MODE_NO_FOCUS);
    }

    void return_focus() { }
};

WfDock::WfDock(WayfireOutput *output)
    : pimpl(new impl(output)) { }
WfDock::~WfDock() = default;

void WfDock::add_child(Gtk::Widget& w) { return pimpl->add_child(w); }
void WfDock::rem_child(Gtk::Widget& w) { return pimpl->rem_child(w); }

wl_surface* WfDock::get_wl_surface() { return pimpl->get_wl_surface(); }
