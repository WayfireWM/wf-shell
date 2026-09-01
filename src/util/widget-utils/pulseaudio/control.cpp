#include "control.hpp"

#include "icon-select.hpp"

#define ICON(volume) icon_from_range(icon_map, volume)

GvcControl::GvcControl(StreamRole role, std::string section) :
    role(role),
    scroll_sensitivity{section + "/volume_scroll_sensitivity"}
{
    if (role == StreamRole::Sink)
    {
        icon_map = volume_icons;
    } else
    {
        icon_map = mic_volume_icons;
    }

    add_css_class("volume-control");
    mute_button.add_css_class("mute-toggle");
    mute_button.add_css_class("flat");
    icon.add_css_class("default-icon");
    mute_button.set_child(icon);
    scale.set_draw_value(false);
    scale.set_size_request(300, 0);
    scale.set_range(0, GvcCommon::get().get_max(role));

    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    signals.push_back(scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        int change = 0;
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = dy * GvcCommon::get().get_max(role) * scroll_sensitivity;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy / 100.0) * GvcCommon::get().get_max(role) * scroll_sensitivity;
        }

        GvcCommon::get().set_volume(role, scale.get_target_value() - change);
        return true;
    }, false));

    scale.add_controller(scroll_gesture);

    append(scale);
    append(mute_button);

    scale.set_range(0.0, GvcCommon::get().get_max(role));

    mute_conn = mute_button.signal_toggled().connect(
        [this, role] ()
    {
        GvcCommon::get().set_muted(role, mute_button.get_active());
    });

    scale.set_user_changed_callback(
        [this, role] ()
    {
        GvcCommon::get().set_volume(role, scale.get_target_value());
    });

    GvcCommon::get().reg_ctrl(this, role);
}

void GvcControl::on_mute(bool newstate)
{
    mute_conn.block(true);
    mute_button.set_active(newstate);
    mute_conn.block(false);
    update_icon();
}

void GvcControl::on_volume(pa_volume_t newstate)
{
    scale.set_target_value(newstate);
    update_icon();
}

double GvcControl::get_scale_target_value()
{
    return scale.get_target_value();
}

void GvcControl::update_icon()
{
    if (GvcCommon::get().get_muted(role))
    {
        add_css_class("muted");
        icon.set_from_icon_name(ICON(0)); // mute
        if (managed_icon)
        {
            managed_icon->set_from_icon_name(ICON(0));
        }

        return;
    }

    remove_css_class("muted");
    icon.set_from_icon_name(ICON(scale.get_target_value() / (double)GvcCommon::get().get_max(role)));
    if (managed_icon)
    {
        managed_icon->set_from_icon_name(ICON(scale.get_target_value() /
            (double)GvcCommon::get().get_max(role)));
    }
}

void GvcControl::set_orientation(Gtk::Orientation orientation)
{
    Box::set_orientation(orientation);
    scale.set_orientation(orientation);
}

StreamRole GvcControl::get_role()
{
    return role;
}

GvcControl::~GvcControl()
{
    GvcCommon::get().unreg_ctrl(this, role);
    mute_conn.disconnect();
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}
