#include "wf-autohide-window.hpp"
#include "display.hpp"
#include <glibmm.h>
#include <iostream>
#include <assert.h>

WayfireAutohidingWindow::WayfireAutohidingWindow(int width, int height,
    WayfireOutput *output, zwf_wm_surface_v1_role role)
{
    this->set_size_request(width, height);
    this->set_decorated(false);
    this->set_resizable(false);

    this->show_all();

    auto surface = this->get_wl_surface();
    if (!surface)
    {
        std::cerr << "Error: created window was not a wayland surface" << std::endl;
        std::exit(-1);
    }

    wm_surface = zwf_shell_manager_v1_get_wm_surface(
        output->display->zwf_shell_manager, surface, role, output->handle);
    this->m_position_changed = [=] () {this->update_position();};

    this->signal_draw().connect_notify(
        [=] (const Cairo::RefPtr<Cairo::Context>&) { update_margin(); });
    this->signal_size_allocate().connect_notify(
        [=] (Gtk::Allocation&) {this->update_exclusive_zone();});

    this->signal_enter_notify_event().connect_notify(
        sigc::mem_fun(this, &WayfireAutohidingWindow::on_enter));
    this->signal_leave_notify_event().connect_notify(
        sigc::mem_fun(this, &WayfireAutohidingWindow::on_leave));

    set_animation_duration(new_static_option("300"));
}

wl_surface* WayfireAutohidingWindow::get_wl_surface() const
{
    auto gdk_window = const_cast<GdkWindow*> (this->get_window()->gobj());
    return gdk_wayland_window_get_wl_surface(gdk_window);
}

zwf_wm_surface_v1* WayfireAutohidingWindow::get_wm_surface() const
{
    return this->wm_surface;
}

void WayfireAutohidingWindow::update_position()
{
    uint32_t anchor = 0;
    if (this->m_position->as_string() == WF_WINDOW_POSITION_TOP) {
        anchor = ZWF_WM_SURFACE_V1_ANCHOR_EDGE_TOP;
    } else if (this->m_position->as_string() == WF_WINDOW_POSITION_BOTTOM) {
        anchor = ZWF_WM_SURFACE_V1_ANCHOR_EDGE_BOTTOM;
    } else {
        std::cerr << "Bad position in config file, defaulting to top" << std::endl;
        anchor = ZWF_WM_SURFACE_V1_ANCHOR_EDGE_TOP;
    }

    zwf_wm_surface_v1_set_anchor(wm_surface, anchor);

    /* When the position changes, show an animation from the new edge. */
    transition.start_value = transition.end_value = get_hidden_y();
    schedule_show(0);
    /* And don't forget to hide the window afterwards, if autohide is enabled */
    if (is_autohide())
        schedule_hide(600);
}

void WayfireAutohidingWindow::set_position(wf_option position)
{
    assert(position);

    if (this->m_position)
        this->m_position->rem_updated_handler(&this->m_position_changed);

    this->m_position = position;
    this->m_position->add_updated_handler(&m_position_changed);
    update_position();
}

void WayfireAutohidingWindow::set_hidden_height(int hidden_height)
{
    this->m_hidden_height = hidden_height;
}

int WayfireAutohidingWindow::get_hidden_y() const
{
    return m_hidden_height - get_allocated_height();
}

void WayfireAutohidingWindow::set_animation_duration(wf_option duration)
{
    /* Make sure we do not lose progress */
    auto current = this->transition.progress();
    auto end = this->transition.end_value;

    this->transition = wf_duration{duration};
    this->transition.start(current, end);
}

void WayfireAutohidingWindow::update_exclusive_zone()
{
    int height = this->get_allocated_height();
    if (!this->has_exclusive_zone)
        height = 0;

    if (height != exclusive_zone)
    {
        exclusive_zone = height;
        zwf_wm_surface_v1_set_exclusive_zone(wm_surface, exclusive_zone);
    }
}

void WayfireAutohidingWindow::set_exclusive_zone(bool exclusive)
{
    this->has_exclusive_zone = exclusive;;
    update_exclusive_zone();
}

void WayfireAutohidingWindow::increase_autohide()
{
    ++autohide_counter;
    if (autohide_counter == 1)
        schedule_hide(0);
}

void WayfireAutohidingWindow::decrease_autohide()
{
    autohide_counter = std::max(autohide_counter - 1, 0);
    if (autohide_counter == 0)
        schedule_show(0);
}

bool WayfireAutohidingWindow::is_autohide() const
{
    return autohide_counter;
}

bool WayfireAutohidingWindow::m_do_hide()
{
    int start = transition.progress();
    transition.start(start, get_hidden_y());
    update_margin();
    return false; // disconnect
}

void WayfireAutohidingWindow::schedule_hide(int delay)
{
    pending_show.disconnect();
    if (delay == 0)
    {
        m_do_hide();
        return;
    }

    if (!pending_hide.connected())
    {
        pending_hide = Glib::signal_timeout().connect(
            sigc::mem_fun(this, &WayfireAutohidingWindow::m_do_hide), delay);
    }
}

bool WayfireAutohidingWindow::m_do_show()
{
    int start = transition.progress();
    transition.start(start, 0);
    update_margin();
    return false; // disconnect
}

void WayfireAutohidingWindow::schedule_show(int delay)
{
    pending_hide.disconnect();
    if (delay == 0)
    {
        m_do_show();
        return;
    }

    if (!pending_show.connected())
    {
        pending_show = Glib::signal_timeout().connect(
            sigc::mem_fun(this, &WayfireAutohidingWindow::m_do_show), delay);
    }
}

bool WayfireAutohidingWindow::update_margin()
{
    if (animation_running || transition.running())
    {
        int target_y = std::round(transition.progress());

        // takes effect only for anchored edges
        zwf_wm_surface_v1_set_margin(wm_surface,
            target_y, target_y, target_y, target_y);

        this->queue_draw();
        return (animation_running = transition.running());
    }

    return false;
}

void WayfireAutohidingWindow::on_enter(GdkEventCrossing *cross)
{
    // ignore events between the window and widgets
    if (cross->detail != GDK_NOTIFY_NONLINEAR &&
        cross->detail != GDK_NOTIFY_NONLINEAR_VIRTUAL)
        return;

    schedule_show(300); // TODO: maybe configurable?
}

void WayfireAutohidingWindow::on_leave(GdkEventCrossing *cross)
{
    // ignore events between the window and widgets
    if (cross->detail != GDK_NOTIFY_NONLINEAR &&
        cross->detail != GDK_NOTIFY_NONLINEAR_VIRTUAL)
        return;

    if (autohide_counter)
        schedule_hide(500);
}

void WayfireAutohidingWindow::set_keyboard_mode(
    zwf_wm_surface_v1_keyboard_focus_mode mode)
{
    zwf_wm_surface_v1_set_keyboard_mode(wm_surface, mode);
}
