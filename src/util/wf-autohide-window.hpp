#ifndef WF_AUTOHIDE_WINDOW_HPP
#define WF_AUTOHIDE_WINDOW_HPP

#include <gtkmm/window.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>
#include <animation.hpp>

struct WayfireOutput;
struct zwf_hotspot_v2;

#define WF_WINDOW_POSITION_TOP    "top"
#define WF_WINDOW_POSITION_BOTTOM "bottom"

struct WayfireAutohidingWindowHotspotCallbacks;
/**
 * A window which is anchored to an edge of the screen, and can autohide.
 *
 * Autohide mode requires that the compositor supports wayfire-shell.
 */
class WayfireAutohidingWindow : public Gtk::Window
{
  public:
    WayfireAutohidingWindow(WayfireOutput *output);
    ~WayfireAutohidingWindow();
    wl_surface* get_wl_surface() const;

    /* Sets the edge of the screen where the window is */
    void set_position(wf_option position);

    /* Sets the time for hiding/showing animation */
    void set_animation_duration(wf_option duration);

    /* Add one more autohide request */
    void increase_autohide();
    /* Remove one autohide request */
    void decrease_autohide();
    /* Returns true if the window is in autohide mode */
    bool is_autohide() const;

    /* Hide or show the panel after delay milliseconds, if nothing happens
     * in the meantime */
    void schedule_hide(int delay);
    void schedule_show(int delay);

    /** When auto exclusive zone is set, the window will adjust its exclusive
     * zone based on the window size.
     *
     * Note that autohide margin isn't taken into account. */
    void set_auto_exclusive_zone(bool has_zone = false);

  private:
    WayfireOutput *output;

    wf_option_callback m_position_changed;
    wf_option m_position;
    void update_position();

    wf_duration transition;
    bool update_margin();

    bool has_auto_exclusive_zone = false;
    int last_zone = 0;

    sigc::connection pending_show, pending_hide;
    bool m_do_show();
    bool m_do_hide();
    int autohide_counter = 0;

    /** Show the window but hide if no pointer input */
    void m_show_uncertain();

    int32_t last_hotspot_height = -1;
    zwf_hotspot_v2 *edge_hotspot = NULL;
    zwf_hotspot_v2 *panel_hotspot = NULL;
    std::unique_ptr<WayfireAutohidingWindowHotspotCallbacks> edge_callbacks;
    std::unique_ptr<WayfireAutohidingWindowHotspotCallbacks> panel_callbacks;
    void setup_hotspot();
};


#endif /* end of include guard: WF_AUTOHIDE_WINDOW_HPP */
