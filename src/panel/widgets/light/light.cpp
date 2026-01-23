#include <gdkmm/monitor.h>
#include <memory>
#include <wayland-client-core.h>

#include "light.hpp"
#include "wf-popover.hpp"

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

WfLightControl::WfLightControl(WayfireLight *_parent){
    parent = _parent;

    // preparation
    scale.set_range(0.0, 1.0);
    scale.set_size_request(300);

    scale.set_user_changed_callback([this](){
        this->set_brightness(scale.get_target_value());
        parent->update_icon();
    });

    // layout
    set_orientation(Gtk::Orientation::VERTICAL);
    append(label);
    append(scale);
}

void WfLightControl::set_scale_target_value(double brightness)
{
    scale.set_target_value(brightness);
}

void WayfireLight::init(Gtk::Box *container){
    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_style_context()->add_class("light");
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
            change = (dy * scroll_sensitivity) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * scroll_sensitivity) / 100;
        }
        if (!invert_scroll)
            change *= -1;

        for (int i = 0 ; i < (int)controls.size() ; i++){
            controls[i]->set_brightness(controls[i]->get_brightness() + change);
        }
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    button->add_controller(scroll_gesture);

    popover->set_child(box);
    popover->get_style_context()->add_class("light-popover");

    container->append(*button);

    box.append(display_ctrl);

    setup_sysfs();

    update_icon();
}

void WayfireLight::add_control(WfLightControl *control){
    auto monitors = button->get_display()->get_monitors();
    Glib::RefPtr<Gdk::Monitor> monitor = monitors->get_typed_object<Gdk::Monitor>(0);
    auto connector = monitor->get_connector();
    if (control->get_name() == connector)
    {
        display_ctrl.append(*control);
    } else
    {
        box.append(*control);
    }
    controls.push_back(control);
}

void WayfireLight::update_icon(){
    // if none, show unavailable
    if (controls.size() == 0){
        icon.set_from_icon_name(brightness_display_icons.at(BRIGHTNESS_LEVEL_OOR));
        return;
    }
    if (icon_target.value() == ICON_TARGET_BRIGHTEST){
        // since brightness is between 0 and 1, we can just start at 0
        double max = 0;
        for (int i = 0 ; i < (int)controls.size() ; i++){
            if (controls[i]->get_brightness() > max){
                max = controls[i]->get_brightness();
            }
        }
        icon.set_from_icon_name(brightness_display_icons.at(light_icon_for(max)));
    }
    if (icon_target.value() == ICON_TARGET_DIMMEST){
        // same as before, but just start from 1
        double min = 1;
        for (int i = 0 ; i < (int)controls.size() ; i++){
            if (controls[i]->get_brightness() > min){
                min = controls[i]->get_brightness();
            }
        }
        icon.set_from_icon_name(brightness_display_icons.at(light_icon_for(min)));
    }
    if (icon_target.value() == ICON_TARGET_AVERAGE){
        double sum = 0;
        for (int i = 0 ; i < (int)controls.size() ; i++){
            sum += controls[i]->get_brightness();
        }
        icon.set_from_icon_name(brightness_display_icons.at(light_icon_for(sum / controls.size())));
    }
}
