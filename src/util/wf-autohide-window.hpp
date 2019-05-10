#ifndef WF_AUTOHIDE_WINDOW_HPP
#define WF_AUTOHIDE_WINDOW_HPP

#include <gtkmm/window.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>
#include <animation.hpp>

#include "wayfire-shell-client-protocol.h"
struct WayfireOutput;

#define WF_WINDOW_POSITION_TOP    "top"
#define WF_WINDOW_POSITION_BOTTOM "bottom"

/* A window which is anchored to an edge of the screen, and can autohide. */
class WayfireAutohidingWindow : public Gtk::Window
{
    zwf_wm_surface_v1 *wm_surface = nullptr;

    wf_option_callback m_position_changed;
    wf_option m_position;
    void update_position();

    bool animation_running = false;
    int m_hidden_height = 1;
    wf_duration transition;
    int get_hidden_y() const;
    bool update_margin();

    bool has_exclusive_zone = false;
    int exclusive_zone = 0;
    void update_exclusive_zone();

    sigc::connection pending_show, pending_hide;
    bool m_do_show();
    bool m_do_hide();
    int autohide_counter = 0;

    void on_enter(GdkEventCrossing *cross);
    void on_leave(GdkEventCrossing *cross);

    public:
        WayfireAutohidingWindow(int width, int height,
            WayfireOutput *output, zwf_wm_surface_v1_role role);
        wl_surface* get_wl_surface() const;
        zwf_wm_surface_v1* get_wm_surface() const;

        /* Sets the edge of the screen where the window is */
        void set_position(wf_option position);

        /* Note: the functions below have effect only if the position is set */
        /* Sets the amount of pixels visible when the window is hidden */
        void set_hidden_height(int hidden_height = 1);
        /* Sets the time for hiding/showing animation */
        void set_animation_duration(wf_option duration);
        /* Sets that the window should have exclusive zone */
        void set_exclusive_zone(bool exclusive = true);

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

        /* Set keyboard mode */
        void set_keyboard_mode(zwf_wm_surface_v1_keyboard_focus_mode mode);
};


#endif /* end of include guard: WF_AUTOHIDE_WINDOW_HPP */
