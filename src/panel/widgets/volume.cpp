#include <gtkmm.h>
#include <glibmm.h>
#include "volume.hpp"

#include "volume-level.hpp"

void WayfireVolume::update_icon()
{
    if (gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream))
    {
        main_image.set_from_icon_name(volume_icon_for(0)); // mute
        return;
    }

    main_image.set_from_icon_name(volume_icon_for(volume_scale.get_target_value() / (double)max_norm));
}

bool WayfireVolume::on_popover_timeout(int timer)
{
    popover.popdown();
    return false;
}

void WayfireVolume::check_set_popover_timeout()
{
    popover_timeout.disconnect();

    popover_timeout = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
        &WayfireVolume::on_popover_timeout), 0), timeout * 1000);
}

void WayfireVolume::set_volume(pa_volume_t volume, set_volume_flags_t flags)
{
    volume_scale.set_target_value(volume);
    if (gvc_stream && (flags & VOLUME_FLAG_UPDATE_GVC))
    {
        gvc_mixer_stream_set_volume(gvc_stream, volume);
        gvc_mixer_stream_push_volume(gvc_stream);
    }

    if (flags & VOLUME_FLAG_SHOW_POPOVER)
    {
        if (!popover.is_visible())
        {
            popover.popup();
        } else
        {
            check_set_popover_timeout();
        }
    }

    update_icon();
}

void WayfireVolume::on_volume_changed_external()
{
    auto volume = gvc_mixer_stream_get_volume(gvc_stream);
    if (volume != (pa_volume_t)this->volume_scale.get_target_value())
    {
        set_volume(volume, VOLUME_FLAG_SHOW_POPOVER);
    }

    check_set_popover_timeout();
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
    /* Setup button */
    signals.push_back(button.signal_clicked().connect([=]
    {
        if (!popover.is_visible())
        {
            popover.popup();
        } else
        {
            popover.popdown();
        }
    }));
    auto style = button.get_style_context();
    style->add_class("widget-icon");
    style->add_class("volume");
    style->add_class("flat");

    gtk_widget_set_parent(GTK_WIDGET(popover.gobj()), GTK_WIDGET(button.gobj()));

    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
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
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    button.add_controller(scroll_gesture);

    /* Setup popover */
    popover.set_autohide(false);
    popover.set_child(volume_scale);
    // popover->set_modal(false);
    popover.add_css_class("volume-popover");
    auto scroll_gesture2 = Gtk::EventControllerScroll::create();
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
    scroll_gesture2->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    scroll_gesture2->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    volume_scale.add_controller(scroll_gesture2);

    volume_scale.set_draw_value(false);
    volume_scale.set_size_request(300, 0);
    volume_scale.set_user_changed_callback([=] () { on_volume_value_changed(); });

    signals.push_back(volume_scale.signal_state_flags_changed().connect(
        [=] (Gtk::StateFlags) { check_set_popover_timeout(); }));

    /* Setup gvc control */
    gvc_control = gvc_mixer_control_new("Wayfire Volume Control");
    notify_default_sink_changed = g_signal_connect(gvc_control,
        "default-sink-changed", G_CALLBACK(default_sink_changed), this);
    gvc_mixer_control_open(gvc_control);

    /* Middle click toggle mute */
    auto middle_click_gesture = Gtk::GestureClick::create();
    auto long_press = Gtk::GestureLongPress::create();
    long_press->set_touch_only(true);
    long_press->signal_pressed().connect(
        [=] (double x, double y)
    {
        bool muted = !(gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream));
        gvc_mixer_stream_change_is_muted(gvc_stream, muted);
        gvc_mixer_stream_push_volume(gvc_stream);
        long_press->set_state(Gtk::EventSequenceState::CLAIMED);
        middle_click_gesture->set_state(Gtk::EventSequenceState::DENIED);
    });
    middle_click_gesture->set_button(2);
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
    button.add_controller(long_press);
    button.add_controller(middle_click_gesture);

    /* Setup layout */
    container->append(button);
    button.set_child(main_image);
}

WayfireVolume::~WayfireVolume()
{
    gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
    disconnect_gvc_stream_signals();

    if (notify_default_sink_changed)
    {
        g_signal_handler_disconnect(gvc_control, notify_default_sink_changed);
    }

    gvc_mixer_control_close(gvc_control);
    g_object_unref(gvc_control);

    popover_timeout.disconnect();
}
