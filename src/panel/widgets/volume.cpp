#include <gtkmm.h>
#include <glibmm.h>

#include "volume.hpp"
#include "wf-popover.hpp"

VolCtrl::VolCtrl(WayfireVolume *widget, StreamRole role, std::string section) :
    GvcControl(role, section),
    widget(widget)
{}

void VolCtrl::on_volume(pa_volume_t newstate)
{
    GvcControl::on_volume(newstate);
    widget->button->popup_timed(timeout * 1000);
}

WayfireVolume::WayfireVolume(StreamRole role) : control{this, role, "panel"}
{}

void WayfireVolume::init(Gtk::Box *container)
{
    control.managed_icon = &main_image;
    control.update_icon();
    main_image.add_css_class("widget-icon");
    button = std::make_unique<WayfireMenuWidget>("panel", "volume");
    button->set_keyboard_interactive(false);

    auto middle_click_gesture = Gtk::GestureClick::create();
    auto long_press     = Gtk::GestureLongPress::create();
    auto scroll_gesture = Gtk::EventControllerScroll::create();

    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);

    long_press->set_touch_only(true);
    middle_click_gesture->set_button(2);

    signals.push_back(scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        int change = 0;
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = dy * GvcCommon::get().get_max(control.get_role()) * scroll_sensitivity;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy / 100.0) * GvcCommon::get().get_max(control.get_role()) * scroll_sensitivity;
        }

        GvcCommon::get().set_volume(control.get_role(), control.get_scale_target_value() - change);
        return true;
    }, true));
    signals.push_back(long_press->signal_pressed().connect(
        [=] (double x, double y)
    {
        GvcCommon::get().set_muted(control.get_role(), !(GvcCommon::get().get_muted(control.get_role())));
        long_press->set_state(Gtk::EventSequenceState::CLAIMED);
        middle_click_gesture->set_state(Gtk::EventSequenceState::DENIED);
    }));
    signals.push_back(middle_click_gesture->signal_pressed().connect([=] (int count, double x, double y)
    {
        middle_click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
    }));
    signals.push_back(middle_click_gesture->signal_released().connect([=] (int count, double x, double y)
    {
        auto muted = GvcCommon::get().get_muted(control.get_role());
        GvcCommon::get().set_muted(control.get_role(), !muted);
    }));

    button->add_controller(scroll_gesture);
    button->add_controller(long_press);
    button->add_controller(middle_click_gesture);
    button->open_on(1);

    /* Setup layout */
    container->append(*button);
    button->set_child(main_image);
    button->set_popup_child(control);
}

WayfireVolume::~WayfireVolume()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}
