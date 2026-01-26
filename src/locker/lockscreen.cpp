#include <iostream>
#include "glib.h"
#include "glibmm/main.h"
#include "locker.hpp"
#include "lockscreen.hpp"


WayfireLockerAppLockscreen::WayfireLockerAppLockscreen()
{
    grid = std::shared_ptr<WayfireLockerGrid>(new WayfireLockerGrid());
    set_child(revealer);
    add_css_class("wf-locker");
    add_css_class("fade-in");
    revealer.set_transition_type(Gtk::RevealerTransitionType::CROSSFADE);
    revealer.set_child(*grid);
    grid->set_expand(true);
    revealer.set_reveal_child(false);

    /* Mouse press or screen touch */
    auto click_gesture = Gtk::GestureClick::create();
    click_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    signals.push_back(click_gesture->signal_released().connect([=] (int count, double x, double y)
    {
        start_disappear_timer();
        /* If we are hidden and have not started showing... */
        if(!revealer.get_reveal_child())
        {
            revealer.set_reveal_child(true);
            click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
        }
    }));
    add_controller(click_gesture);

    /* Mouse movement */
    auto pointer_gesture = Gtk::EventControllerMotion::create();
    signals.push_back(pointer_gesture->signal_motion().connect([=] (double x, double y)
    {
        // Avoid first motion event and repeated with same location
        int ix = x;
        int iy = y;
        if (last_x < 0 && last_y < 0)
        {
            last_x = ix;
            last_y = iy;
            return;
        }
        if (ix == last_x && iy == last_y)
        {
            return;
        }
        last_x = ix;
        last_y = iy;
        start_disappear_timer();
        if(!revealer.get_reveal_child())
        {
            revealer.set_reveal_child(true);
        }
    }));
    add_controller(pointer_gesture);
    /* Keypress */
    auto typing_gesture = Gtk::EventControllerKey::create();
    typing_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    signals.push_back(typing_gesture->signal_key_pressed().connect([=] (guint keyval, guint keycode,
                                                      Gdk::ModifierType state)
    {
        start_disappear_timer();
        if(!revealer.get_reveal_child())
        {
            revealer.set_reveal_child(true);
            return true;
        }
        return false;
    }, false));
    add_controller(typing_gesture);
}

void WayfireLockerAppLockscreen::start_disappear_timer()
{
    WayfireLockerApp::get().user_activity();
    if (timeout)
    {
        timeout.disconnect();
    }
    if (hide_timeout>0)
    {
        timeout = Glib::signal_timeout().connect_seconds(
                [this] ()
        {
            revealer.set_reveal_child(false);
            return G_SOURCE_REMOVE;
        }, 5);
    }
}

void WayfireLockerAppLockscreen::disconnect()
{
    timeout.disconnect();
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}