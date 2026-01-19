#include <pipewire/keys.h>
#include <gtkmm.h>

#include "wf-wp-control.hpp"
#include "../volume-level.hpp"

WfWpControl::WfWpControl(WpPipewireObject *obj, WayfireWpMixer *parent_widget)
{
    object = obj;
    parent = parent_widget;
}

WfWpControl::~WfWpControl()
{
    mute_conn.disconnect();
    middle_conn.disconnect();
    right_conn.disconnect();
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

void WfWpControl::init()
{
    guint32 id = wp_proxy_get_bound_id(WP_PROXY(object));

    // build layout

    button.set_child(volume_icon);
    button.get_style_context()->add_class("wireplumber");
    button.get_style_context()->add_class("flat");

    scale.set_range(0.0, 1.0);
    scale.set_size_request(slider_length, 0);

    const gchar *name;
    // try to find a name to display
    name = wp_pipewire_object_get_property(object, PW_KEY_NODE_DESCRIPTION);
    if (!name)
    {
        name = wp_pipewire_object_get_property(object, PW_KEY_NODE_NICK);
    }

    if (!name)
    {
        name = wp_pipewire_object_get_property(object, PW_KEY_NODE_NAME);
    }

    if (!name)
    {
        name = "Unnamed";
    }

    label.set_text(Glib::ustring(name));
    // this causes the label’s *minimum* size to be 0, causing it to not request any space,
    // thus having the column’s width be dictated solely by the scale’s size request
    label.set_max_width_chars(0);
    label.set_ellipsize(Pango::EllipsizeMode::END);

    set_tooltip_text(name);

    update_icons_pos();

    // setup user interactions

    signals.push_back(mute_conn = button.signal_toggled().connect(
        [this, id] ()
    {
        // if the menu was popped up because of an external change
        // and is now changed manually, don’t hide
        parent->cancel_popover_timeout();
        ignore = true;
        // disregarding the return value prevents cases where ignore is read at an ncorrect time,
        // with the only problem of having one update ignored if something goes very wrong
        WpCommon::get().set_mute(id, button.get_active());
    }));

    scale.set_user_changed_callback(
        [this, id] ()
    {
        parent->cancel_popover_timeout(); // see above
        ignore = true;
        WpCommon::get().set_volume(id, scale.get_target_value());
    });

    auto scroll_gesture = Gtk::EventControllerScroll::create();
    signals.push_back(scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        dy = parent->invert_scroll ? dy : dy * -1; // for the same scrolling as volume widget, which we will
                                                   // agree it is more intuitive for more people
        double change = 0;
        const double SCROLL_MULT = 0.2; // corrects the scrolling to have the default scroll sensitivity as 1
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = (dy * parent->scroll_sensitivity * SCROLL_MULT) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * parent->scroll_sensitivity * SCROLL_MULT) / 100;
        }

        WpCommon::get().set_volume(id, scale.get_target_value() + change);
        return true;
    }, true));
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    add_controller(scroll_gesture);

    middle_click_mute = Gtk::GestureClick::create();
    right_click_mute  = Gtk::GestureClick::create();
    middle_click_mute->set_button(2);
    right_click_mute->set_button(3);
    add_controller(middle_click_mute);
    add_controller(right_click_mute);

    update_gestures();

    // initialise the values
    auto v = WpCommon::get().get_volume_and_mute(id);
    set_scale_target_value(v.first);
    set_btn_status_no_callbk(v.second);

    update_icon();
}

void WfWpControl::set_btn_status_no_callbk(bool state)
{
    mute_conn.block(true);
    button.set_active(state);
    mute_conn.block(false);
}

void WfWpControl::set_scale_target_value(double volume)
{
    scale.set_target_value(volume);
}

void WfWpControl::update_icon()
{
    if (button.get_active())
    {
        volume_icon.set_from_icon_name(volume_icon_for(0)); // mute
        return;
    }

    volume_icon.set_from_icon_name(volume_icon_for(get_scale_target_value()));
}

double WfWpControl::get_scale_target_value()
{
    return scale.get_target_value();
}

// attaches elements to the grid at the appropriate place
void WfWpControl::update_icons_pos()
{
    static WfOption<bool> icons_on_left{"panel/wp_icons_on_left"};

    if (icons_on_left)
    {
        attach(label, 1, 0);
        attach(scale, 1, 1);
        attach(button, 0, 1);
    } else
    {
        attach(label, 0, 0);
        attach(scale, 0, 1);
        attach(button, 1, 1);
    }
}

void WfWpControl::update_gestures()
{
    // the setting says that it is for quick target, but let’s have all the muting consistent
    static WfOption<std::string> str_wp_right_click_action{"panel/wp_right_click_action"};
    static WfOption<std::string> str_wp_middle_click_action{"panel/wp_middle_click_action"};

    auto mute_action =
        [&] (int count, double x, double y)
    {
        button.set_active(!button.get_active());
    };

    if (str_wp_middle_click_action.value() == "mute_quick_target")
    {
        middle_conn = middle_click_mute->signal_pressed().connect(mute_action);
    }

    if (str_wp_right_click_action.value() == "mute_quick_target")
    {
        right_conn = right_click_mute->signal_pressed().connect(mute_action);
    }
}

void WfWpControl::handle_config_reload()
{
    remove(label);
    remove(scale);
    remove(button);
    middle_conn.disconnect();
    right_conn.disconnect();
    update_icons_pos();
    update_gestures();
    scale.set_size_request(slider_length);
}

// used to make a copy to the face of the widget
std::unique_ptr<WfWpControl> WfWpControl::copy()
{
    return std::make_unique<WfWpControl>(object, parent);
}

WfWpControlDevice::~WfWpControlDevice()
{
    def_conn.disconnect();
}

void WfWpControlDevice::init()
{
    default_btn.get_style_context()->add_class("wireplumber");
    default_btn.get_style_context()->add_class("flat");

    is_def_icon.set_from_icon_name("emblem-default");
    default_btn.set_child(is_def_icon);

    // we are not using ToggleButton groups because on_default_nodes_changed
    // will be called anyway to set the status of all devices
    def_conn = default_btn.signal_clicked().connect(
        [this] ()
    {
        // keep the button down when it is selected to prevent inconsistency of visuals with actual state
        if (default_btn.get_active() == false)
        {
            set_def_status_no_callbk(true);
            return;
        }

        WpCommon::get().set_default(object);
    });

    WfWpControl::init();
}

void WfWpControlDevice::set_def_status_no_callbk(bool state)
{
    def_conn.block(true);
    default_btn.set_active(state);
    def_conn.block(false);
}

void WfWpControlDevice::update_icons_pos()
{
    WfWpControl::update_icons_pos();
    static WfOption<bool> icons_on_left{"panel/wp_icons_on_left"};

    if (icons_on_left)
    {
        attach(default_btn, 0, 0);
    } else
    {
        attach(default_btn, 1, 0);
    }
}

void WfWpControlDevice::handle_config_reload()
{
    remove(default_btn);
    WfWpControl::handle_config_reload();
}
