#include "light.hpp"
#include "wf-popover.hpp"
#include <memory>

static BrightnessLevel light_icon_for(double value)
{
    double max = 1.0;
    auto third = max / 3;
    if (value <= third)
    {
        return BRIGHTNESS_LEVEL_LOW;
    } else if ((value > third) && (value <= (third * 2)))
    {
        return BRIGHTNESS_LEVEL_MEDIUM;
    } else if ((value > (third * 2)) && (value <= max))
    {
        return BRIGHTNESS_LEVEL_HIGH;
    }

    return BRIGHTNESS_LEVEL_OOR;
}

WfLightControl::WfLightControl(){
    // preparation
    scale.set_range(0.0, 1.0);
    scale.set_size_request(300);

    scale.set_user_changed_callback([this](){
        this->set_brightness(scale.get_target_value());
    });

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

    // scroll to brighten and dim all monitors
    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        double change = 0;

        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = (dy * 1/* scroll_sensitivity */) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * 1/* scroll_sensitivity */) / 100;
        }
        // if (!invert_scroll)
            change *= -1;

        for (int i = 0 ; i < controls.size() ; i++){
            controls[i]->set_brightness(controls[i]->get_brightness() + change);
        }
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    button->add_controller(scroll_gesture);

    popover->set_child(box);
    popover->get_style_context()->add_class("light-popover");

    container->append(*button);

    setup_fs();
}

void WayfireLight::add_control(std::unique_ptr<WfLightControl> control){
    box.append(*control);
    controls.push_back(std::move(control));
}

// void WayfireLight::update_icon(){}
