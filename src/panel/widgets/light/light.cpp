#include <gdkmm/monitor.h>
#include <memory>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "light.hpp"
#include "wf-popover.hpp"
#include "wf-shell-app.hpp"

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
    parent->update_icon();
}
double WfLightControl::get_scale_target_value()
{
    return scale.get_target_value();
}

WayfireLight::WayfireLight(WayfireOutput *_output)
{
    output = _output;
}

void WayfireLight::init(Gtk::Box *container){
    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_style_context()->add_class("light");
    button->get_style_context()->add_class("flat");
    button->set_child(icon);
    button->show();
    popover = button->get_popover();
    popover->set_autohide(false);

    // layout
    box.append(display_box);
    box.set_orientation(Gtk::Orientation::VERTICAL);

    disp_othr_sep.set_orientation(Gtk::Orientation::HORIZONTAL);
    box.append(disp_othr_sep);
    box.append(other_box);

    display_label.set_text("This monitor");
    display_box.append(display_label);
    display_box.set_orientation(Gtk::Orientation::VERTICAL);

    other_label.set_text("Other monitors");
    other_box.append(other_label);
    other_box.set_orientation(Gtk::Orientation::VERTICAL);

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
            controls[i]->set_brightness(controls[i]->get_scale_target_value() + change);
        }
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    button->add_controller(scroll_gesture);

    popover->set_child(box);
    popover->get_style_context()->add_class("light-popover");

    container->append(*button);

    setup_sysfs();

    update_icon();
}

void WayfireLight::add_control(WfLightControl *control){
    auto connector = output->monitor->get_connector();
    if (control->get_name() == connector)
    {
        ctrl_this_display = control;
        display_box.append(*control);
    } else
    {
        box.append(*control);
    }
    controls.push_back(control);
}

void WayfireLight::update_icon(){
    // if none, show unavailable
    if (!ctrl_this_display){
        icon.set_from_icon_name(brightness_display_icons.at(BRIGHTNESS_LEVEL_OOR));
        return;
    }

    icon.set_from_icon_name(brightness_display_icons.at(
        light_icon_for(ctrl_this_display->get_scale_target_value()))
    );
}
