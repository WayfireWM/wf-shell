#ifndef WF_AUTOHIDE_WINDOW_HPP
#define WF_AUTOHIDE_WINDOW_HPP

#include <gtkmm/window.h>
#include <gdk/wayland/gdkwayland.h>
#include <wayfire/util/duration.hpp>

#include "wf-option-wrap.hpp"
#include "wf-popover.hpp"

struct WayfireOutput;
struct zwf_hotspot_v2;

#define WF_WINDOW_POSITION_TOP    "top"
#define WF_WINDOW_POSITION_BOTTOM "bottom"
#define WF_WINDOW_POSITION_LEFT   "left"
#define WF_WINDOW_POSITION_RIGHT  "right"

struct WayfireAutohidingWindowHotspotCallbacks;
/**
 * A window which is anchored to an edge of the screen, and can autohide.
 *
 * Autohide mode requires that the compositor supports wayfire-shell.
 */
class WayfireAutohidingWindow : public Gtk::Window
{
  public:
    /**
     * WayfireAutohidingWindow's behavior can be modified with several config
     * file options:
     *
     * 1. section/position
     * 2. section/full_span
     * 3. section/minimal_height
     * 4. section/minimal_width
     * 5. section/autohide
     * 6. section/autohide_duration
     * 7. section/autohide_show_delay
     * 8. section/autohide_hide_delay
     * 9. section/edge_hotspot_size
     * 10.section/adjacent_edge_hotspot_size
     */
    WayfireAutohidingWindow(WayfireOutput *output, const std::string& section);
    WayfireAutohidingWindow(WayfireAutohidingWindow&&) = delete;
    WayfireAutohidingWindow(const WayfireAutohidingWindow&) = delete;
    WayfireAutohidingWindow& operator =(const WayfireAutohidingWindow&) = delete;
    WayfireAutohidingWindow& operator =(WayfireAutohidingWindow&&) = delete;

    ~WayfireAutohidingWindow();
    wl_surface *get_wl_surface() const;

    /* Add one more autohide request */
    void increase_autohide();
    /* Remove one autohide request */
    void decrease_autohide();
    /* Returns true if the window should autohide */
    bool should_autohide() const;

    /* Hide or show the panel after delay milliseconds, if nothing happens
     * in the meantime */
    void schedule_hide(int delay);
    void schedule_show(int delay);

    /** When auto exclusive zone is set, the window will adjust its exclusive
     * zone based on the window size.
     *
     * Note that autohide margin isn't taken into account. */
    void set_auto_exclusive_zone(bool has_zone = false);

    /**
     * Set the currently active popover button.
     * The lastly activated popover, if any, will be closed, in order to
     * show this new one.
     *
     * In addition, if the window has an active popover, it will grab the
     * keyboard input and deactivate the popover when the focus is lost.
     */
    void set_active_popover(WayfireMenuWidget& button);

    /**
     * No-op if the given popover is not the currently active popover.
     *
     * Unsets the currently active popover and reverses the effects of setting
     * making it active with set_active_popover()
     */
    void unset_active_popover(WayfireMenuWidget& popover);

    void unset_active_popover();

    /*
     * Get Active popover or null
     */
    WayfireMenuWidget *get_active_popover();

  private:
    WayfireOutput *output;

    std::vector<sigc::connection> signals;

    WfOption<std::string> position;
    WfOption<bool> full_span;
    void update_position();

    WfOption<int> minimal_width;
    WfOption<int> minimal_height;

    wf::animation::simple_animation_t autohide_animation;
    int (Gtk::Widget::*get_allocated_height_or_width)() const;
    bool update_margin();

    WfOption<bool> autohide_opt;
    bool last_autohide_value = autohide_opt;
    void setup_autohide();
    void update_autohide();

    WfOption<int> autohide_show_delay;
    WfOption<int> autohide_hide_delay;

    WfOption<int> edge_margin;

    bool auto_exclusive_zone     = !autohide_opt;
    int auto_exclusive_zone_size = 0;
    void setup_auto_exclusive_zone();
    void update_auto_exclusive_zone();

    WfOption<int> edge_hotspot_size, adjacent_edge_hotspot_size;
    int last_edge_hotspot_size = 0, last_adjacent_edge_hotspot_size = 0;
    int last_edge_offset = -1;

    sigc::connection pending_show, pending_hide;
    bool m_do_show();
    bool m_do_hide();

    void start_draw_timer();
    gboolean update_animation(Glib::RefPtr<Gdk::FrameClock> fc);
    int autohide_counter = static_cast<int>(autohide_opt);

    /** Show the window but hide if no pointer input */
    void m_show_uncertain();

    bool input_inside_panel = false;
    zwf_hotspot_v2 *edge_hotspot = NULL, *panel_hotspot = NULL;
    std::vector<zwf_hotspot_v2*> adjacent_edges_hotspots;
    std::unique_ptr<WayfireAutohidingWindowHotspotCallbacks> edge_callbacks, adjacent_edge_callbacks,
        panel_callbacks;
    void reinit_ext_hotspots();
    void setup_hotspot();

    sigc::connection popover_hide;
    WayfireMenuWidget *active_button = nullptr;
};


#endif /* end of include guard: WF_AUTOHIDE_WINDOW_HPP */
