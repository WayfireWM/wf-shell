#ifndef WF_AUTOHIDE_WINDOW_HPP
#define WF_AUTOHIDE_WINDOW_HPP

#include <gtkmm/window.h>
#include <gdk/wayland/gdkwayland.h>
#include "wf-popover.hpp"
#include <wf-option-wrap.hpp>
#include <wayfire/util/duration.hpp>

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
     * WayfireAutohidingWindow's behavior can be modified with several config file options:
     * 1. section/position
     * 2. section/full_span
     * 3. section/autohide_duration
     * 4. section/edge_offset
     * 5. section/minimal_height
     * 6. section/minimal_width
     * 7. section/autohide
     * 8. section/autohide_show_delay
     * 9. section/autohide_hide_delay
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

    /* Hide or show the panel after delay milliseconds, if nothing happens in the meantime */
    void schedule_hide(int delay);
    void schedule_show(int delay);

    /** When auto exclusive zone is set, the window will adjust its exclusive
     * zone based on the window size.
     *
     * Note that autohide margin isn't taken into account. */
    void set_auto_exclusive_zone(bool has_zone = false);

    /**
     * Set the currently active popover button.
     * The lastly activated popover, if any, will be closed,
     * in order to show this new one.
     *
     * In addition, if the window has an active popover, it will grab the
     * keyboard input and deactivate the popover when the focus is lost.
     */
    void set_active_popover(WayfireMenuButton& button);

    /**
     * No-op if the given popover is not the currently active popover.
     *
     * Unsets the currently active popover and reverses the effects of setting
     * making it active with set_active_popover()
     */
    void unset_active_popover(WayfireMenuButton& popover);

  private:
    WayfireOutput *output;

    WfOption<std::string> position;
    WfOption<bool> full_span;
    void update_position();

    wf::animation::simple_animation_t autohide_animation;
    int (Gtk::Widget::*get_allocated_height_or_width)() const;
    bool update_margin();

    WfOption<int> edge_offset;
    int last_edge_offset = -1;

    WfOption<int> minimal_height;
    WfOption<int> minimal_width;

    WfOption<bool> autohide_opt;
    WfOption<int> autohide_show_delay;
    WfOption<int> autohide_hide_delay;

    bool last_autohide_value = autohide_opt;
    void setup_autohide();
    void update_autohide();

    bool auto_exclusive_zone     = !autohide_opt;
    int auto_exclusive_zone_size = 0;
    void setup_auto_exclusive_zone();
    void update_auto_exclusive_zone();

    sigc::connection pending_show, pending_hide;
    bool m_do_show();
    bool m_do_hide();

    void start_draw_timer();
    gboolean update_animation(Glib::RefPtr<Gdk::FrameClock> fc);
    int autohide_counter = static_cast<int>(autohide_opt);

    /** Show the window but hide if no pointer input */
    void m_show_uncertain();

    std::string last_position = "";
    int32_t last_hotspot_size = -1;
    bool input_inside_panel   = false;
    zwf_hotspot_v2 *edge_hotspot  = NULL;
    zwf_hotspot_v2 *panel_hotspot = NULL;
    std::unique_ptr<WayfireAutohidingWindowHotspotCallbacks> edge_callbacks;
    std::unique_ptr<WayfireAutohidingWindowHotspotCallbacks> panel_callbacks;
    void setup_hotspot();

    sigc::connection popover_hide;
    WayfireMenuButton *active_button = nullptr;
};


#endif /* end of include guard: WF_AUTOHIDE_WINDOW_HPP */
