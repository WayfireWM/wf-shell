#include "wf-autohide-window.hpp"
#include "wayfire-shell-unstable-v2-client-protocol.h"

#include <gtk-layer-shell.h>
#include <wf-shell-app.hpp>
#include <gdk/gdkwayland.h>

#include <glibmm.h>
#include <iostream>
#include <assert.h>

#define AUTOHIDE_SHOW_DELAY 300
#define AUTOHIDE_HIDE_DELAY 500

WayfireAutohidingWindow::WayfireAutohidingWindow(WayfireOutput *output,
    const std::string& section) :
    position{section + "/position"},
    y_position{WfOption<int>{section + "/autohide_duration"}}
{
    this->output = output;
    this->set_decorated(false);
    this->set_resizable(false);

    gtk_layer_init_for_window(this->gobj());
    gtk_layer_set_monitor(this->gobj(), output->monitor->gobj());
    gtk_layer_set_namespace(this->gobj(), "$unfocus panel");

    this->position.set_callback([=] () { this->update_position(); });
    this->update_position();

    this->signal_draw().connect_notify(
        [=] (const Cairo::RefPtr<Cairo::Context>&) { update_margin(); });

    this->signal_size_allocate().connect_notify(
        [=] (Gtk::Allocation&) {
            this->set_auto_exclusive_zone(this->has_auto_exclusive_zone);
            this->setup_hotspot();
        });

    this->signal_focus_out_event().connect_notify(
        [=] (const GdkEventFocus*) {
            if (this->active_button)
                unset_active_popover(*this->active_button);
        });
}

WayfireAutohidingWindow::~WayfireAutohidingWindow()
{
    if (this->edge_hotspot)
        zwf_hotspot_v2_destroy(this->edge_hotspot);
    if (this->panel_hotspot)
        zwf_hotspot_v2_destroy(this->panel_hotspot);
}

wl_surface* WayfireAutohidingWindow::get_wl_surface() const
{
    auto gdk_window = const_cast<GdkWindow*> (this->get_window()->gobj());
    return gdk_wayland_window_get_wl_surface(gdk_window);
}

/** Verify that position is correct and return a correct position */
static std::string check_position(std::string position)
{
    if (position == WF_WINDOW_POSITION_TOP)
        return WF_WINDOW_POSITION_TOP;
    if (position == WF_WINDOW_POSITION_BOTTOM)
        return WF_WINDOW_POSITION_BOTTOM;

    std::cerr << "Bad position in config file, defaulting to top" << std::endl;
    return WF_WINDOW_POSITION_TOP;
}

static GtkLayerShellEdge get_anchor_edge(std::string position)
{
    position = check_position(position);
    if (position == WF_WINDOW_POSITION_TOP)
        return GTK_LAYER_SHELL_EDGE_TOP;
    if (position == WF_WINDOW_POSITION_BOTTOM)
        return GTK_LAYER_SHELL_EDGE_BOTTOM;

    assert(false); // not reached because check_position()
}

void WayfireAutohidingWindow::m_show_uncertain()
{
    schedule_show(16); // add some delay to finish setting up the window
    /* And don't forget to hide the window afterwards, if autohide is enabled */
    if (should_autohide())
    {
        pending_hide = Glib::signal_timeout().connect([=] () {
            schedule_hide(0);
            return false;
        }, AUTOHIDE_HIDE_DELAY);
    }
}

void WayfireAutohidingWindow::update_position()
{
    /* Reset old anchors */
    gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_TOP, false);
    gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, false);

    /* Set new anchor */
    GtkLayerShellEdge anchor = get_anchor_edge(position);
    gtk_layer_set_anchor(this->gobj(), anchor, true);

    /* When the position changes, show an animation from the new edge. */
    y_position.animate(-this->get_allocated_height(), -this->get_allocated_height());
    setup_hotspot();
    m_show_uncertain();
}

struct WayfireAutohidingWindowHotspotCallbacks
{
    std::function<void()> on_enter;
    std::function<void()> on_leave;
};

static void handle_hotspot_enter(void *data, zwf_hotspot_v2*)
{
    auto cb = (WayfireAutohidingWindowHotspotCallbacks*) data;
    cb->on_enter();
}

static void handle_hotspot_leave(void *data, zwf_hotspot_v2*)
{
    auto cb = (WayfireAutohidingWindowHotspotCallbacks*) data;
    cb->on_leave();
}

static zwf_hotspot_v2_listener hotspot_listener = {
    .enter = handle_hotspot_enter,
    .leave = handle_hotspot_leave,
};

/**
 * An autohide window needs 2 hotspots.
 * One of them is used to trigger autohide and is generally a tiny strip on the
 * edge of the output.
 *
 * The other hotspot covers the whole window. It is used primarily to know when
 * the input leaves the window, in which case we need to hide the window again.
 */

void WayfireAutohidingWindow::setup_hotspot()
{
    if (!this->output->output)
        return;

    /* No need to recreate hotspots if the height didn't change */
    if (this->get_allocated_height() == last_hotspot_height)
        return;
    this->last_hotspot_height = get_allocated_height();

    if (this->edge_hotspot)
        zwf_hotspot_v2_destroy(edge_hotspot);
    if (this->panel_hotspot)
        zwf_hotspot_v2_destroy(panel_hotspot);

    auto position = check_position(this->position);
    uint32_t edge = (position == WF_WINDOW_POSITION_TOP) ?
        ZWF_OUTPUT_V2_HOTSPOT_EDGE_TOP : ZWF_OUTPUT_V2_HOTSPOT_EDGE_BOTTOM;

    // TODO: make edge offset configurable
    this->edge_hotspot = zwf_output_v2_create_hotspot(output->output,
        edge, 20, AUTOHIDE_SHOW_DELAY);

    this->panel_hotspot = zwf_output_v2_create_hotspot(output->output,
        edge, this->get_allocated_height(), 0); // immediate

    this->edge_callbacks =
        std::make_unique<WayfireAutohidingWindowHotspotCallbacks> ();
    this->panel_callbacks =
        std::make_unique<WayfireAutohidingWindowHotspotCallbacks> ();

    edge_callbacks->on_enter = [=] () {
        schedule_show(0);
    };

    edge_callbacks->on_leave = [=] () {
        // nothing
    };

    this->input_inside_panel = false;
    panel_callbacks->on_enter = [=] () {
        if (this->pending_hide.connected())
            this->pending_hide.disconnect();
        this->input_inside_panel = true;
    };

    panel_callbacks->on_leave = [=] () {
        this->input_inside_panel = false;
        if (this->should_autohide())
            this->schedule_hide(AUTOHIDE_HIDE_DELAY);
    };

    zwf_hotspot_v2_add_listener(edge_hotspot, &hotspot_listener,
        edge_callbacks.get());
    zwf_hotspot_v2_add_listener(panel_hotspot, &hotspot_listener,
        panel_callbacks.get());
}

void WayfireAutohidingWindow::set_auto_exclusive_zone(bool has_zone)
{
    this->has_auto_exclusive_zone = has_zone;
    int target_zone = has_zone ? get_allocated_height() : 0;

    if (this->last_zone != target_zone)
    {
        gtk_layer_set_exclusive_zone(this->gobj(), target_zone);
        last_zone = target_zone;
    }
}

void WayfireAutohidingWindow::increase_autohide()
{
    ++autohide_counter;
    if (should_autohide())
        schedule_hide(0);
}

void WayfireAutohidingWindow::decrease_autohide()
{
    autohide_counter = std::max(autohide_counter - 1, 0);
    if (!should_autohide())
        schedule_show(0);
}

bool WayfireAutohidingWindow::should_autohide() const
{
    return autohide_counter && !this->active_button && !this->input_inside_panel;
}

bool WayfireAutohidingWindow::m_do_hide()
{
    y_position.animate(-get_allocated_height());
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
    y_position.animate(0);
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
    if (y_position.running())
    {
        gtk_layer_set_margin(this->gobj(),
            get_anchor_edge(position), y_position);

        this->queue_draw(); // XXX: is this necessary?
        return true;
    }

    return false;
}

void WayfireAutohidingWindow::set_active_popover(WayfireMenuButton& button)
{
    if (&button != this->active_button)
    {
        if (this->active_button)
        {
            this->popover_hide.disconnect();
            this->active_button->set_active(false);
            this->active_button->get_popover()->popdown();
        }

        this->active_button = &button;
        this->popover_hide =
            this->active_button->m_popover.signal_hide().connect_notify(
                [this, &button] () { unset_active_popover(button); });
    }

    bool should_grab_focus = this->active_button->is_keyboard_interactive();
    gtk_layer_set_keyboard_interactivity(this->gobj(), should_grab_focus);
    this->active_button->set_has_focus(should_grab_focus);
    schedule_show(0);
}

void WayfireAutohidingWindow::unset_active_popover(WayfireMenuButton& button)
{
    if (!this->active_button || &button != this->active_button)
        return;

    this->active_button->set_has_focus(false);
    this->active_button->set_active(false);
    this->active_button->get_popover()->popdown();
    this->active_button = nullptr;
    this->popover_hide.disconnect();

    gtk_layer_set_keyboard_interactivity(this->gobj(), false);

    if (should_autohide())
        schedule_hide(AUTOHIDE_HIDE_DELAY);
}
