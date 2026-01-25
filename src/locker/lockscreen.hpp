#pragma once
#include <gtkmm.h>
#include <giomm.h>
#include <memory>
#include <sigc++/connection.h>

#include "gtkmm/eventcontroller.h"
#include "gtkmm/gesture.h"
#include "wf-option-wrap.hpp"
#include "lockergrid.hpp"
class WayfireLockerAppLockscreen: public Gtk::Window
{
    public:
    std::shared_ptr<WayfireLockerGrid> grid;
    Gtk::Revealer revealer;
    sigc::connection timeout;
    WfOption<int> hide_timeout {"locker/hide_time"};

    WayfireLockerAppLockscreen()
    {
        grid = std::shared_ptr<WayfireLockerGrid>(new WayfireLockerGrid());
        set_child(revealer);
        add_css_class("wf-locker");
        revealer.set_transition_type(Gtk::RevealerTransitionType::CROSSFADE);
        revealer.set_child(*grid);
        grid->set_expand(true);
        revealer.set_reveal_child(false);

        /* Mouse press or screen touch */
        auto click_gesture = Gtk::GestureClick::create();
        click_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
        click_gesture->signal_released().connect([=] (int count, double x, double y)
        {
            start_disappear_timer();
            /* If we are hidden and have not started showing... */
            if(!revealer.get_reveal_child())
            {
                revealer.set_reveal_child(true);
                click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
            }
        });
        add_controller(click_gesture);

        /* Mouse movement */
        auto pointer_gesture = Gtk::EventControllerMotion::create();
        pointer_gesture->signal_motion().connect([=] (double x, double y)
        {
            start_disappear_timer();
            /* If we are hidden and have not started showing... */
            if(!revealer.get_reveal_child())
            {
                revealer.set_reveal_child(true);
            }
        });
        add_controller(pointer_gesture);
        /* Keypress */
        auto typing_gesture = Gtk::EventControllerKey::create();
        typing_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
        typing_gesture->signal_key_pressed().connect([=] (guint keyval, guint keycode,
                                                          Gdk::ModifierType state)
        {
            start_disappear_timer();
            if(!revealer.get_reveal_child())
            {
                revealer.set_reveal_child(true);
                return true;
            }
            return false;
        }, false);
        add_controller(typing_gesture);
    }

    void start_disappear_timer()
    {
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
};