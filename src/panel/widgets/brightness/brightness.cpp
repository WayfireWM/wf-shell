#include <gdkmm/monitor.h>
#include <memory>
#include <pulse/proplist.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "brightness.hpp"
#include "wf-popover.hpp"
#include "wf-shell-app.hpp"
#include "icon-select.hpp"

#define ICON(brightness) icon_from_range(brightness_display_icons, brightness)

WfLightControl::WfLightControl(WayfireBrightness *_parent)
{
    parent = _parent;

    scale.set_range(0.0, 1.0);
    slider_length.set_callback([=] ()
    {
        scale.set_size_request(slider_length.value());
    });
    scale.set_size_request(slider_length.value());

    scale.set_user_changed_callback([this] ()
    {
        this->set_brightness(scale.get_target_value());
    });

    set_orientation(Gtk::Orientation::VERTICAL);
    append(label);
    append(scale);

    // scroll
    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        double change = 0;

        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = (dy * parent->scroll_sensitivity) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * parent->scroll_sensitivity) / 100;
        }

        if (!(parent->invert_scroll))
        {
            change *= -1;
        }

        // correct for a "good feeling" change at sensitivity 1
        change *= 0.2;

        double newv = get_scale_target_value() + change;
        newv = std::clamp(newv, 0.0, 1.0);
        set_brightness(newv);
        set_scale_target_value(newv);
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    add_controller(scroll_gesture);
}

WayfireBrightness*WfLightControl::get_parent()
{
    return parent;
}

void WfLightControl::set_scale_target_value(double brightness)
{
    scale.set_target_value(brightness);
    update_parent_icon();
}

double WfLightControl::get_scale_target_value()
{
    return scale.get_target_value();
}

void WfLightControl::update_parent_icon()
{
    if (parent->ctrl_this_display.get() == this)
    {
        parent->update_icon();
        if (parent->popup_on_change)
        {
            parent->button->popup_timed(parent->popup_timeout);
        }
    }
}

void LightManager::add_widget(WayfireBrightness *widget)
{
    widgets.push_back(widget);
    catch_up_widget(widget);
}

void LightManager::rem_widget(WayfireBrightness *widget)
{
    strip_widget(widget);
    widgets.erase(find(widgets.begin(), widgets.end(), widget));
}

WayfireBrightness::WayfireBrightness(WayfireOutput *_output)
{
    output = _output;
}

WayfireBrightness::~WayfireBrightness()
{
    SysfsSurveillor::get().rem_widget(this);
#ifdef HAVE_DDCUTIL
    DdcaSurveillor::get().rem_widget(this);
#endif
}

void WayfireBrightness::init(Gtk::Box *container)
{
    button = std::make_unique<WayfireMenuWidget>("panel", "brightness");
    button->set_child(icon);
    button->show();
    button->open_on(1);
    button->add_css_class("widget-icon");

    // layout
    box.set_orientation(Gtk::Orientation::VERTICAL);

    auto set_spacing = [=] ()
    {
        box.set_spacing(spacing);
        display_box.set_spacing(spacing);
        other_box.set_spacing(spacing);
    };
    set_spacing();
    spacing.set_callback(set_spacing);

    display_label.set_text("This monitor");
    display_box.append(display_label);
    display_box.set_orientation(Gtk::Orientation::VERTICAL);
    display_box.add_css_class("this-monitor");

    disp_othr_sep.set_orientation(Gtk::Orientation::HORIZONTAL);
    box.append(disp_othr_sep);

    other_label.set_text("Other monitors");
    other_box.append(other_label);
    other_box.set_orientation(Gtk::Orientation::VERTICAL);
    other_box.add_css_class("other-monitors");

    box.append(other_box);

    // scroll to brighten and dim the monitor the panel is on
    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        if (!ctrl_this_display)
        {
            return false;
        }

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
        {
            change *= -1;
        }

        // correct for a "good feeling" change at sensitivity 1
        change *= 0.2;

        double newv = ctrl_this_display->get_scale_target_value() + change;
        newv = std::clamp(newv, 0.0, 1.0);
        ctrl_this_display->set_brightness(newv);
        ctrl_this_display->set_scale_target_value(newv);
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    button->add_controller(scroll_gesture);

    button->set_popup_child(box);

    container->append(*button);

    SysfsSurveillor::get().add_widget(this);
#ifdef HAVE_DDCUTIL
    DdcaSurveillor::get().add_widget(this);
#endif

    update_icon();
}

void WayfireBrightness::hide_unused()
{
    // hide unused boxes and the separator if any
    bool disp_show = ctrl_this_display != nullptr;
    bool othr_show = other_box.get_children().size() != 1; // there is always the label

    if (disp_show)
    {
        display_box.show();
    } else
    {
        display_box.hide();
    }

    if (othr_show)
    {
        other_box.show();
    } else
    {
        other_box.hide();
    }

    if (disp_show && othr_show)
    {
        disp_othr_sep.show();
    } else
    {
        disp_othr_sep.hide();
    }
}

void WayfireBrightness::add_control(std::shared_ptr<WfLightControl> control)
{
    auto connector = output->monitor->get_connector();
    if (control->get_connector() == connector)
    {
        if (!ctrl_this_display)
        {
            box.prepend(display_box);
            ctrl_this_display = std::shared_ptr(control);
            display_box.append(*control);

            update_icon();
        }
    } else
    {
        other_box.append(*control);
    }

    hide_unused();

    controls.push_back(control);
}

void WayfireBrightness::rem_control(std::shared_ptr<WfLightControl> control)
{
    if (control == ctrl_this_display)
    {
        display_box.remove(*control);
        box.remove(display_box);
        ctrl_this_display = nullptr;
        update_icon();
    } else
    {
        other_box.remove(*control);
    }

    hide_unused();

    controls.erase(std::remove_if(controls.begin(), controls.end(),
        [control] (const std::shared_ptr<WfLightControl>& c) { return c == control; }),
        controls.end());
}

void WayfireBrightness::update_icon()
{
    // if none, show unavailable
    if (!ctrl_this_display)
    {
        icon.set_from_icon_name(ICON(-1));
        return;
    }

    icon.set_from_icon_name(ICON(ctrl_this_display->get_scale_target_value()));
}
