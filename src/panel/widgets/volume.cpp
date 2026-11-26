#include <gtkmm.h>
#include <glibmm.h>
#include <cmath>
#include <string>

#include "volume.hpp"
#include "icon-select.hpp"
#include "wf-popover.hpp"

#define ICON(volume) icon_from_range(volume_icons, volume)

void WayfireVolume::update_icon()
{
    if (gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream))
    {
        main_image.set_from_icon_name(ICON(0)); // mute
        volume_tooltip_label.set_text("Volume: Muted");
        return;
    }

    double volume = volume_scale.get_target_value() / (double)max_norm;

    main_image.set_from_icon_name(ICON(volume));
    volume_tooltip_label.set_text("Volume: " +
        std::to_string(std::lround(volume * 100)) + "%");
}

void WayfireVolume::set_volume(pa_volume_t volume, set_volume_flags_t flags)
{
    volume_scale.set_target_value(volume);
    if (gvc_stream && (flags & VOLUME_FLAG_UPDATE_GVC))
    {
        gvc_mixer_stream_set_volume(gvc_stream, volume);
        gvc_mixer_stream_push_volume(gvc_stream);
    }

    update_icon();
}

void WayfireVolume::on_volume_changed_external()
{
    auto volume = gvc_mixer_stream_get_volume(gvc_stream);
    if (volume != (pa_volume_t)this->volume_scale.get_target_value())
    {
        set_volume(volume, VOLUME_FLAG_SHOW_POPOVER);

        if (popup_on_change.value())
        {
            Glib::signal_idle().connect([=] ()
            {
                button->popup_timed(timeout * 1000);
                return G_SOURCE_REMOVE;
            });
        }
    }
}

static void notify_volume(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume*)user_data;
    wf_volume->on_volume_changed_external();
}

static void notify_is_muted(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume*)user_data;
    wf_volume->update_icon();
}

void WayfireVolume::disconnect_gvc_stream_signals()
{
    if (notify_volume_signal)
    {
        g_signal_handler_disconnect(gvc_stream, notify_volume_signal);
    }

    notify_volume_signal = 0;

    if (notify_is_muted_signal)
    {
        g_signal_handler_disconnect(gvc_stream, notify_is_muted_signal);
    }

    notify_is_muted_signal = 0;
}

void WayfireVolume::on_default_sink_changed()
{
    gvc_stream = gvc_mixer_control_get_default_sink(gvc_control);
    if (!gvc_stream)
    {
        printf("GVC: Failed to get default sink\n");
        return;
    }

    /* Reconnect signals to new sink */
    disconnect_gvc_stream_signals();
    notify_volume_signal = g_signal_connect(gvc_stream, "notify::volume",
        G_CALLBACK(notify_volume), this);
    notify_is_muted_signal = g_signal_connect(gvc_stream, "notify::is-muted",
        G_CALLBACK(notify_is_muted), this);

    /* Update the scale attributes */
    max_norm = gvc_mixer_control_get_vol_max_norm(gvc_control);
    volume_scale.set_range(0.0, max_norm);
    volume_scale.set_increments(max_norm * scroll_sensitivity,
        max_norm * scroll_sensitivity * 2);

    /* Finally, update the displayed volume. However, do not show the popup */
    set_volume(gvc_mixer_stream_get_volume(gvc_stream), VOLUME_FLAG_NO_ACTION);
}

static void default_sink_changed(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume*)user_data;
    wf_volume->on_default_sink_changed();
}

void WayfireVolume::on_volume_value_changed()
{
    /* User manually changed volume */
    set_volume(volume_scale.get_target_value());
}

void WayfireVolume::init(Gtk::Box *container)
{
    main_image.add_css_class("widget-icon");
    button = std::make_unique<WayfireMenuWidget>("panel", "volume");
    button->set_keyboard_interactive(false);

    volume_tooltip.set_autohide(false);
    volume_tooltip.set_has_arrow(false);
    volume_tooltip.set_can_target(false);
    auto panel_position_changed = [this] ()
    {
        auto pos = panel_position.value();
        if (pos == "top")
        {
            volume_tooltip.set_position(Gtk::PositionType::BOTTOM);
        } else if (pos == "bottom")
        {
            volume_tooltip.set_position(Gtk::PositionType::TOP);
        } else if (pos == "left")
        {
            volume_tooltip.set_position(Gtk::PositionType::RIGHT);
        } else if (pos == "right")
        {
            volume_tooltip.set_position(Gtk::PositionType::LEFT);
        }
    };

    panel_position_changed();
    panel_position.set_callback(panel_position_changed);
    volume_tooltip.add_css_class("volume-tooltip");
    volume_tooltip.set_child(volume_tooltip_label);
    volume_tooltip.set_parent(*button);

    auto tooltip_motion = Gtk::EventControllerMotion::create();

    signals.push_back(tooltip_motion->signal_enter().connect([this] (double, double)
    {
        tooltip_delay.disconnect();
        tooltip_delay = Glib::signal_timeout().connect([this] ()
        {
            if (!button->is_popup_visible())
            {
                volume_tooltip.popup();
            }

            return false;
        }, 500);
    }));

    signals.push_back(tooltip_motion->signal_leave().connect([this] ()
    {
        tooltip_delay.disconnect();
        volume_tooltip.popdown();
    }));

    button->add_controller(tooltip_motion);

    signals.push_back(button->signal_popup().connect([this] ()
    {
        tooltip_delay.disconnect();
        volume_tooltip.popdown();
    }));

    auto middle_click_gesture = Gtk::GestureClick::create();
    auto long_press     = Gtk::GestureLongPress::create();
    auto scroll_gesture = Gtk::EventControllerScroll::create();
    auto scroll_gesture2 = Gtk::EventControllerScroll::create();

    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    scroll_gesture2->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_gesture2->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);

    long_press->set_touch_only(true);
    middle_click_gesture->set_button(2);

    /* Setup gvc control */
    gvc_control = gvc_mixer_control_new("Wayfire Volume Control");
    notify_default_sink_changed = g_signal_connect(gvc_control,
        "default-sink-changed", G_CALLBACK(default_sink_changed), this);
    gvc_mixer_control_open(gvc_control);

    signals.push_back(scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        int change = 0;
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = dy * max_norm * scroll_sensitivity;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy / 100.0) * max_norm * scroll_sensitivity;
        }

        set_volume(std::clamp(volume_scale.get_target_value() - change,
            0.0, max_norm));

        tooltip_delay.disconnect();

        if (!button->is_popup_visible() && !volume_tooltip.get_visible())
        {
            volume_tooltip.popup();
        }

        return true;
    }, true));
    signals.push_back(scroll_gesture2->signal_scroll().connect([=] (double dx, double dy)
    {
        int change = 0;
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = dy * max_norm * scroll_sensitivity;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy / 100.0) * max_norm * scroll_sensitivity;
        }

        set_volume(std::clamp(volume_scale.get_target_value() - change,
            0.0, max_norm));
        return true;
    }, false));
    signals.push_back(long_press->signal_pressed().connect(
        [=] (double x, double y)
    {
        bool muted = !(gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream));
        gvc_mixer_stream_change_is_muted(gvc_stream, muted);
        gvc_mixer_stream_push_volume(gvc_stream);
        long_press->set_state(Gtk::EventSequenceState::CLAIMED);
        middle_click_gesture->set_state(Gtk::EventSequenceState::DENIED);
    }));
    signals.push_back(middle_click_gesture->signal_pressed().connect([=] (int count, double x, double y)
    {
        middle_click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
    }));
    signals.push_back(middle_click_gesture->signal_released().connect([=] (int count, double x, double y)
    {
        bool muted = !(gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream));
        gvc_mixer_stream_change_is_muted(gvc_stream, muted);
        gvc_mixer_stream_push_volume(gvc_stream);
    }));

    volume_scale.set_draw_value(false);
    volume_scale.set_size_request(300, 0);
    volume_scale.set_user_changed_callback([=] () { on_volume_value_changed(); });
    volume_scale.add_controller(scroll_gesture2);
    button->add_controller(scroll_gesture);
    button->add_controller(long_press);
    button->add_controller(middle_click_gesture);
    button->open_on(1);

    /* Setup layout */
    container->append(*button);
    button->set_child(main_image);
    button->set_popup_child(volume_scale);
    /* Override scroll bar as it isn't needed here */
    button->get_scroll().set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::NEVER);
}

WayfireVolume::~WayfireVolume()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }

    tooltip_delay.disconnect();
    volume_tooltip.popdown();
    volume_tooltip.unparent();

    disconnect_gvc_stream_signals();

    if (notify_default_sink_changed)
    {
        g_signal_handler_disconnect(gvc_control, notify_default_sink_changed);
    }

    gvc_mixer_control_close(gvc_control);
    g_object_unref(gvc_control);
}
