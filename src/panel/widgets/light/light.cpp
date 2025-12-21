#include "light.hpp"
#include "wf-popover.hpp"

WfLightControl::WfLightControl(){
    // preparation
    scale.set_range(0.0, 1.0);
    scale.set_target_value(0.5);
    scale.set_size_request(300);

    scale.set_user_changed_callback([this](){
        this->set_brightness(scale.get_target_value());
    });

    const std::string name;

    label.set_text(get_name());

    // layout
    set_orientation(Gtk::Orientation::VERTICAL);
    append(label);
    append(scale);
}

void WayfireLight::init(Gtk::Box *container){
    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_style_context()->add_class("flat");
    button->set_child(icon);
    button->show();
    popover = button->get_popover();
    popover->set_autohide(false);

    // scroll to change brighten and dim all monitors
    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        double change = 0;
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = (dy * scroll_sensitivity) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * scroll_sensitivity) / 100;
        }
        // for (auto control: controls){
            // control->set_brightness(control->get_brightness() + change);
        // }
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    button->add_controller(scroll_gesture);

    popover->set_child(box);
    popover->get_style_context()->add_class("light-popover");

    container->append(*button);
}
