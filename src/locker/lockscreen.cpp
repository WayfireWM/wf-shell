#include "locker.hpp"
#include "lockscreen.hpp"


WayfireLockerAppLockscreen::WayfireLockerAppLockscreen()
{
    grid = std::shared_ptr<WayfireLockerGrid>(new WayfireLockerGrid());
    set_child(*grid);
    add_css_class("wf-locker");
    grid->set_expand(true);

    /* Mouse press or screen touch */
    auto click_gesture = Gtk::GestureClick::create();
    click_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    signals.push_back(click_gesture->signal_released().connect([=] (int count, double x, double y)
    {
        window_activity();
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
        window_activity();
    }));
    add_controller(pointer_gesture);
    /* Keypress */
    auto typing_gesture = Gtk::EventControllerKey::create();
    typing_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    signals.push_back(typing_gesture->signal_key_pressed().connect([=] (guint keyval, guint keycode,
                                                      Gdk::ModifierType state)
    {
        window_activity();
        return false;
    }, false));
    add_controller(typing_gesture);
}


void WayfireLockerAppLockscreen::window_activity()
{
    // Alert entire locker, in case of pre-wake
    WayfireLockerApp::get().user_activity();
    // Alert all widgets in window, to reveal themselves
    grid->window_activity();
}
void WayfireLockerAppLockscreen::disconnect()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}