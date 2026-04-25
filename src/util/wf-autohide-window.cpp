#include "wf-autohide-window.hpp"
#include "wayfire-shell-unstable-v2-client-protocol.h"
#include "wf-popover.hpp"

#include <gtk4-layer-shell.h>
#include <wf-shell-app.hpp>
#include <gdk/wayland/gdkwayland.h>
#include <gtkmm.h>

#include <glibmm.h>
#include <iostream>
#include <assert.h>

WayfireAutohidingWindow::WayfireAutohidingWindow(WayfireOutput *output,
    const std::string& section) :
    force_show_popup{"panel/force_show_popup"},
    position{section + "/position"},
    full_span{section + "/full_span"},
    minimal_width{section + "/minimal_width"},
    minimal_height{section + "/minimal_height"},
    autohide_animation{WfOption<int>{section + "/autohide_duration"}},
    autohide_opt{section + "/autohide"},
    autohide_show_delay{section + "/autohide_show_delay"},
    autohide_hide_delay{section + "/autohide_hide_delay"},
    edge_margin{section + "/edge_margin"},
    edge_hotspot_size{section + "/edge_hotspot_size"},
    adjacent_edge_hotspot_size{section + "/adjacent_edge_hotspot_size"}
{
    this->output = output;
    this->set_decorated(false);

    gtk_layer_init_for_window(this->gobj());
    gtk_layer_set_monitor(this->gobj(), output->monitor->gobj());
    gtk_layer_set_namespace(this->gobj(), "panel");


    this->position.set_callback([=] () { this->update_position(); });
    this->full_span.set_callback([=] () { this->update_position(); });
    this->edge_margin.set_callback([=] () { update_position(); });
    this->update_position();

    const auto set_size = [=] () { this->set_size_request(minimal_width, minimal_height); };
    this->minimal_height.set_callback(set_size);
    this->minimal_width.set_callback(set_size);
    set_size();

    auto pointer_gesture = Gtk::EventControllerMotion::create();
    signals.push_back(pointer_gesture->signal_enter().connect([=] (double x, double y)
    {
        if (this->pending_hide.connected())
        {
            this->pending_hide.disconnect();
        }

        this->input_inside_panel = true;
        autohide_animation.animate(0);
        start_draw_timer();
    }));
    signals.push_back(pointer_gesture->signal_leave().connect([=]
    {
        this->input_inside_panel = false;
        if (this->should_autohide())
        {
            this->schedule_hide(autohide_hide_delay);
        }
    }));
    this->add_controller(pointer_gesture);

    this->setup_autohide();

    this->edge_hotspot_size.set_callback([=] () { this->setup_hotspot(); });

    this->autohide_opt.set_callback([=] { setup_autohide(); });

    auto display  = Gdk::Display::get_default();
    auto monitors = display->get_monitors();
    signals.push_back(monitors->signal_items_changed().connect([=] (auto, auto, auto)
    {
        reinit_ext_hotspots();
    }));

    // wait for idle, so once everything is initialised. Else, things depend on loading order.
    Glib::signal_idle().connect_once([=] () { this->reinit_ext_hotspots(); });

    if (!output->output)
    {
        std::cerr << "WARNING: Compositor does not support zwf_shell_manager_v2 " << \
            "disabling hotspot and autohide features " << \
            "(is wayfire-shell plugin enabled?)" << std::endl;
        return;
    }

    static const zwf_output_v2_listener listener = {
        .enter_fullscreen = [] (void *data, zwf_output_v2*)
        {},
        .leave_fullscreen = [] (void *data, zwf_output_v2*)
        {},
        .toggle_menu = [] (void *data, zwf_output_v2*)
        {
            ((WayfireAutohidingWindow*)data)->output->toggle_menu_signal().emit();
        },
    };
    zwf_output_v2_add_listener(output->output, &listener, this);
}

WayfireAutohidingWindow::~WayfireAutohidingWindow()
{
    if (this->edge_hotspot)
    {
        zwf_hotspot_v2_destroy(this->edge_hotspot);
    }

    if (this->panel_hotspot)
    {
        zwf_hotspot_v2_destroy(this->panel_hotspot);
    }

    for (auto adjacent_edge_hotspot : adjacent_edges_hotspots)
    {
        zwf_hotspot_v2_destroy(adjacent_edge_hotspot);
    }

    for (auto handler : signals)
    {
        handler.disconnect();
    }
}

wl_surface*WayfireAutohidingWindow::get_wl_surface() const
{
    auto wsurf = GDK_WAYLAND_SURFACE(this->get_surface()->gobj());
    return gdk_wayland_surface_get_wl_surface(wsurf);
}

/** Verify that position is correct and return a correct position */
static std::string check_position(std::string position)
{
    if (position == WF_WINDOW_POSITION_TOP)
    {
        return WF_WINDOW_POSITION_TOP;
    }

    if (position == WF_WINDOW_POSITION_BOTTOM)
    {
        return WF_WINDOW_POSITION_BOTTOM;
    }

    if (position == WF_WINDOW_POSITION_LEFT)
    {
        return WF_WINDOW_POSITION_LEFT;
    }

    if (position == WF_WINDOW_POSITION_RIGHT)
    {
        return WF_WINDOW_POSITION_RIGHT;
    }

    std::cerr << "Bad position in config file, defaulting to top" << std::endl;
    return WF_WINDOW_POSITION_TOP;
}

static GtkLayerShellEdge get_anchor_edge(std::string position)
{
    position = check_position(position);
    if (position == WF_WINDOW_POSITION_TOP)
    {
        return GTK_LAYER_SHELL_EDGE_TOP;
    }

    if (position == WF_WINDOW_POSITION_BOTTOM)
    {
        return GTK_LAYER_SHELL_EDGE_BOTTOM;
    }

    if (position == WF_WINDOW_POSITION_LEFT)
    {
        return GTK_LAYER_SHELL_EDGE_LEFT;
    }

    if (position == WF_WINDOW_POSITION_RIGHT)
    {
        return GTK_LAYER_SHELL_EDGE_RIGHT;
    }

    assert(false); // not reached because check_position()
}

void WayfireAutohidingWindow::m_show_uncertain()
{
    schedule_show(16); // add some delay to finish setting up the window
    /* And don't forget to hide the window afterwards, if autohide is enabled */
    if (should_autohide())
    {
        pending_hide = Glib::signal_timeout().connect([=] ()
        {
            schedule_hide(0);
            return false;
        }, autohide_hide_delay);
    }
}

void WayfireAutohidingWindow::update_position()
{
    /* Reset old anchors */
    gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_TOP, false);
    gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, false);
    gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, false);
    gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, false);

    /* Set new anchor */
    GtkLayerShellEdge edge = get_anchor_edge(position);
    gtk_layer_set_anchor(this->gobj(), edge, true);
    gtk_layer_set_margin(this->gobj(), edge, edge_margin);

    if (full_span)
    {
        if ((edge == GTK_LAYER_SHELL_EDGE_TOP) || (edge == GTK_LAYER_SHELL_EDGE_BOTTOM))
        {
            gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
            gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
        } else if ((edge == GTK_LAYER_SHELL_EDGE_LEFT) || (edge == GTK_LAYER_SHELL_EDGE_RIGHT))
        {
            gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
            gtk_layer_set_anchor(this->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
        }
    }

    if (!output->output)
    {
        return;
    }

    // need different measurements depending on position
    if ((edge == GTK_LAYER_SHELL_EDGE_LEFT) || (edge == GTK_LAYER_SHELL_EDGE_RIGHT))
    {
        get_allocated_height_or_width = &Gtk::Widget::get_allocated_width;
    } else
    {
        get_allocated_height_or_width = &Gtk::Widget::get_allocated_height;
    }

    /* When the position changes, show an animation from the new edge. */
    autohide_animation.animate(-(this->*get_allocated_height_or_width)(), 0);
    start_draw_timer();
    m_show_uncertain();
    setup_hotspot();
}

struct WayfireAutohidingWindowHotspotCallbacks
{
    std::function<void()> on_enter;
    std::function<void()> on_leave;
};

static void handle_hotspot_enter(void *data, zwf_hotspot_v2*)
{
    auto cb = (WayfireAutohidingWindowHotspotCallbacks*)data;
    cb->on_enter();
}

static void handle_hotspot_leave(void *data, zwf_hotspot_v2*)
{
    auto cb = (WayfireAutohidingWindowHotspotCallbacks*)data;
    cb->on_leave();
}

static zwf_hotspot_v2_listener hotspot_listener = {
    .enter = handle_hotspot_enter,
    .leave = handle_hotspot_leave,
};

void WayfireAutohidingWindow::reinit_ext_hotspots()
{
    for (auto adjacent_edge_hotspot : adjacent_edges_hotspots)
    {
        zwf_hotspot_v2_destroy(adjacent_edge_hotspot);
    }

    adjacent_edges_hotspots.clear();

    uint32_t adjacent_edge;
    auto position = check_position(this->position);
    if (position == WF_WINDOW_POSITION_TOP)
    {
        adjacent_edge = ZWF_OUTPUT_V2_HOTSPOT_EDGE_BOTTOM;
    } else if (position == WF_WINDOW_POSITION_BOTTOM)
    {
        adjacent_edge = ZWF_OUTPUT_V2_HOTSPOT_EDGE_TOP;
    } else if (position == WF_WINDOW_POSITION_LEFT)
    {
        adjacent_edge = ZWF_OUTPUT_V2_HOTSPOT_EDGE_RIGHT;
    } else if (position == WF_WINDOW_POSITION_RIGHT)
    {
        adjacent_edge = ZWF_OUTPUT_V2_HOTSPOT_EDGE_LEFT;
    }

    Gdk::Rectangle geom_this;
    this->output->monitor->get_geometry(geom_this);
    auto pos = check_position(this->position);

    std::map<WayfireOutput*, zwf_output_v2_hotspot_edge> wo_edg;

    for (auto& wo : *WayfireShellApp::get().get_wayfire_outputs())
    {
        if (!wo->output || (wo.get() == this->output))
        {
            continue;
        }

        Gdk::Rectangle other_geo;
        wo->monitor->get_geometry(other_geo);

        if (pos == WF_WINDOW_POSITION_TOP)
        {
            if (other_geo.get_y() + other_geo.get_height() == geom_this.get_y())
            {
                wo_edg[wo.get()] = ZWF_OUTPUT_V2_HOTSPOT_EDGE_BOTTOM;
            }
        } else if (pos == WF_WINDOW_POSITION_BOTTOM)
        {
            if (other_geo.get_y() == geom_this.get_y() + geom_this.get_height())
            {
                wo_edg[wo.get()] = ZWF_OUTPUT_V2_HOTSPOT_EDGE_TOP;
            }
        } else if (pos == WF_WINDOW_POSITION_LEFT)
        {
            if (other_geo.get_x() + other_geo.get_width() == geom_this.get_x())
            {
                wo_edg[wo.get()] = ZWF_OUTPUT_V2_HOTSPOT_EDGE_RIGHT;
            }
        } else if (pos == WF_WINDOW_POSITION_RIGHT)
        {
            if (other_geo.get_x() == geom_this.get_x() + geom_this.get_width())
            {
                wo_edg[wo.get()] = ZWF_OUTPUT_V2_HOTSPOT_EDGE_LEFT;
            }
        }
    }

    for (auto pair : wo_edg)
    {
        adjacent_edges_hotspots.push_back(zwf_output_v2_create_hotspot(pair.first->output,
            adjacent_edge, adjacent_edge_hotspot_size, autohide_show_delay));
    }

    adjacent_edge_callbacks->on_enter = [=] ()
    {
        schedule_show(0);
    };

    adjacent_edge_callbacks->on_leave = [=] ()
    {
        m_do_hide();
    };

    for (auto ad_ed_ht : adjacent_edges_hotspots)
    {
        zwf_hotspot_v2_add_listener(ad_ed_ht, &hotspot_listener,
            adjacent_edge_callbacks.get());
    }
}

/**
 * An autohide window needs 2 hotspots.
 * One of them is used to trigger autohide and is generally a tiny strip on the
 * edge of the output.
 *
 * The other hotspot covers the whole window. It is used primarily to know when
 * the input leaves the window, in which case we need to hide the window again.
 *
 * A last one is optional and is placed as a mirror of the first one on an
 * adjacent monitor, and serves to have more room to pop out the window
 */

void WayfireAutohidingWindow::setup_hotspot()
{
    if (this->edge_hotspot)
    {
        zwf_hotspot_v2_destroy(edge_hotspot);
    }

    if (this->panel_hotspot)
    {
        zwf_hotspot_v2_destroy(panel_hotspot);
    }

    auto position = check_position(this->position);
    uint32_t edge;
    if (position == WF_WINDOW_POSITION_TOP)
    {
        edge = ZWF_OUTPUT_V2_HOTSPOT_EDGE_TOP;
    } else if (position == WF_WINDOW_POSITION_BOTTOM)
    {
        edge = ZWF_OUTPUT_V2_HOTSPOT_EDGE_BOTTOM;
    } else if (position == WF_WINDOW_POSITION_LEFT)
    {
        edge = ZWF_OUTPUT_V2_HOTSPOT_EDGE_LEFT;
    } else if (position == WF_WINDOW_POSITION_RIGHT)
    {
        edge = ZWF_OUTPUT_V2_HOTSPOT_EDGE_RIGHT;
    }

    this->edge_hotspot = zwf_output_v2_create_hotspot(output->output,
        edge, edge_hotspot_size, autohide_show_delay);

    this->panel_hotspot = zwf_output_v2_create_hotspot(output->output,
        edge, (this->*get_allocated_height_or_width)(), 0); // immediate

    this->edge_callbacks =
        std::make_unique<WayfireAutohidingWindowHotspotCallbacks>();
    this->adjacent_edge_callbacks =
        std::make_unique<WayfireAutohidingWindowHotspotCallbacks>();
    this->panel_callbacks =
        std::make_unique<WayfireAutohidingWindowHotspotCallbacks>();

    edge_callbacks->on_enter = [=] ()
    {
        schedule_show(0);
    };

    edge_callbacks->on_leave = [=] ()
    {
        // nothing
    };

    this->input_inside_panel  = false;
    panel_callbacks->on_enter = [=] ()
    {
        if (this->pending_hide.connected())
        {
            this->pending_hide.disconnect();
        }

        this->input_inside_panel = true;
    };

    panel_callbacks->on_leave = [=] ()
    {
        this->input_inside_panel = false;
        if (this->should_autohide())
        {
            this->schedule_hide(autohide_hide_delay);
        }
    };

    zwf_hotspot_v2_add_listener(edge_hotspot, &hotspot_listener,
        edge_callbacks.get());

    zwf_hotspot_v2_add_listener(panel_hotspot, &hotspot_listener,
        panel_callbacks.get());
}

void WayfireAutohidingWindow::setup_auto_exclusive_zone()
{
    if (!auto_exclusive_zone && (auto_exclusive_zone == 0))
    {
        return;
    }

    this->update_auto_exclusive_zone();
}

void WayfireAutohidingWindow::update_auto_exclusive_zone()
{
    if (this->auto_exclusive_zone)
    {
        gtk_layer_auto_exclusive_zone_enable(this->gobj());
    } else
    {
        gtk_layer_set_exclusive_zone(this->gobj(), 0);
    }
}

void WayfireAutohidingWindow::set_auto_exclusive_zone(bool has_zone)
{
    if (has_zone && (output->output && autohide_opt))
    {
        std::cerr << "WARNING: Trying to enable auto_exclusive_zone with " <<
            "autohide enabled might look jarring; preventing it." << std::endl;
        return;
    }

    auto_exclusive_zone = has_zone;
    update_auto_exclusive_zone();
}

void WayfireAutohidingWindow::increase_autohide()
{
    ++autohide_counter;
    if (should_autohide())
    {
        schedule_hide(0);
    }
}

void WayfireAutohidingWindow::decrease_autohide()
{
    autohide_counter = std::max(autohide_counter - 1, 0);
    if (!should_autohide())
    {
        schedule_show(0);
    }
}

bool WayfireAutohidingWindow::should_autohide() const
{
    return autohide_counter && !this->active_button && !this->input_inside_panel;
}

bool WayfireAutohidingWindow::m_do_hide()
{
    autohide_animation.animate(-((this->*get_allocated_height_or_width)() + edge_margin));
    start_draw_timer();
    update_margin();
    return false; // disconnect
}

void WayfireAutohidingWindow::start_draw_timer()
{
    add_tick_callback(sigc::mem_fun(*this, &WayfireAutohidingWindow::update_animation));
}

gboolean WayfireAutohidingWindow::update_animation(Glib::RefPtr<Gdk::FrameClock> frame_clock)
{
    update_margin();
    // this->queue_draw();
    // Once we've finished fading, stop this callback
    return autohide_animation.running() ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
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
            sigc::mem_fun(*this, &WayfireAutohidingWindow::m_do_hide), delay);
    }
}

bool WayfireAutohidingWindow::m_do_show()
{
    autohide_animation.animate(0);
    start_draw_timer();
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
            sigc::mem_fun(*this, &WayfireAutohidingWindow::m_do_show), delay);
    }
}

bool WayfireAutohidingWindow::update_margin()
{
    if (autohide_animation.running())
    {
        gtk_layer_set_margin(this->gobj(),
            get_anchor_edge(position), edge_margin + autohide_animation);
        // queue_draw does not work when the panel is hidden
        // so calling wl_surface_commit to make WM show the panel back
        if (get_surface())
        {
            wl_surface_commit(get_wl_surface());
        }

        this->queue_draw();
        return true;
    }

    return false;
}

bool WayfireAutohidingWindow::is_active_popover(WayfireMenuWidget& button)
{
    return this->active_button == &button;
}

bool WayfireAutohidingWindow::has_active_popover()
{
    return this->active_button != nullptr;
}

void WayfireAutohidingWindow::set_active_popover(WayfireMenuWidget& button)
{
    if (!is_active_popover(button))
    {
        this->active_button = &button;
    }

    /* Set this panel to forcibly show over fullscreen apps */
    if (force_show_popup.value())
    {
        gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
    }

    const bool should_grab_focus = this->active_button->is_keyboard_interactive();
    if (should_grab_focus)
    {
        gtk_layer_set_keyboard_mode(this->gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    } else
    {
        gtk_layer_set_keyboard_mode(this->gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    }

    schedule_show(0);
}

void WayfireAutohidingWindow::unset_active_popover()
{
    if (this->active_button)
    {
        this->active_button = nullptr;
        this->popover_hide.disconnect();
    }

    gtk_layer_set_keyboard_mode(this->gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    if (should_autohide())
    {
        schedule_hide(AUTOHIDE_HIDE_DELAY);
    }

    WfOption<std::string> panel_layer{"panel/layer"};

    if ((std::string)panel_layer == "overlay")
    {
        gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
    }

    if ((std::string)panel_layer == "top")
    {
        gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_TOP);
    }

    if ((std::string)panel_layer == "bottom")
    {
        gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_BOTTOM);
    }

    if ((std::string)panel_layer == "background")
    {
        gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    }
}

WayfireMenuWidget*WayfireAutohidingWindow::get_active_popover()
{
    return this->active_button;
}

void WayfireAutohidingWindow::setup_autohide()
{
    if (!output->output && autohide_opt)
    {
        std::cerr << "WARNING: Attempting to enable autohide, but the " <<
            "compositor does not support zwf_shell_manager_v2; ignoring" <<
            "autohide (is compositor's wayfire-shell plugin enabled?)" <<
            std::endl;
    }

    this->set_auto_exclusive_zone(!(output->output && autohide_opt));
    this->update_autohide();
}

void WayfireAutohidingWindow::update_autohide()
{
    if (!autohide_opt)
    {
        schedule_show(0);
    }

    if ((output->output && autohide_opt) == last_autohide_value)
    {
        return;
    }

    if (output->output && autohide_opt)
    {
        increase_autohide();
    } else
    {
        decrease_autohide();
    }

    last_autohide_value = output->output && autohide_opt;
    setup_auto_exclusive_zone();
}
