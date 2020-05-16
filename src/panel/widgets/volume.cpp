#include <iostream>
#include <glibmm.h>
#include "volume.hpp"
#include "launchers.hpp"
#include "gtk-utils.hpp"

#define INCREMENT_STEP_PC 0.05

WayfireVolumeScale::WayfireVolumeScale()
{
    this->signal_draw().connect_notify(
        [=] (const Cairo::RefPtr<Cairo::Context>& ctx)
        {
            if (this->current_volume.running())
            {
                value_changed.block();
                this->set_value(this->current_volume);
                value_changed.unblock();
            }
        }, true);

    value_changed = this->signal_value_changed().connect_notify([=] () {
        this->current_volume.animate(this->get_value(), this->get_value());
        if (this->user_changed_callback)
            this->user_changed_callback();
    });
}

void WayfireVolumeScale::set_target_value(double value)
{
    this->current_volume.animate(value);
    this->queue_draw();
}

double WayfireVolumeScale::get_target_value() const
{
    return this->current_volume;
}

void WayfireVolumeScale::set_user_changed_callback(
    std::function<void()> callback)
{
    this->user_changed_callback = callback;
}

enum VolumeLevel {
    VOLUME_LEVEL_MUTE = 0,
    VOLUME_LEVEL_LOW,
    VOLUME_LEVEL_MED,
    VOLUME_LEVEL_HIGH,
    VOLUME_LEVEL_OOR /* Out of range */
};

static VolumeLevel get_volume_level(pa_volume_t volume, pa_volume_t max)
{
    auto third = max / 3;
    if (volume == 0)
        return VOLUME_LEVEL_MUTE;
    else if (volume > 0 && volume <= third)
        return VOLUME_LEVEL_LOW;
    else if (volume > third && volume <= (third * 2))
        return VOLUME_LEVEL_MED;
    else if (volume > (third * 2) && volume <= max)
        return VOLUME_LEVEL_HIGH;

    return VOLUME_LEVEL_OOR;
}

void WayfireVolume::update_icon()
{
    VolumeLevel current =
        get_volume_level(volume_scale.get_target_value(), max_norm);

    if (gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream))
    {
        main_image.set_from_icon_name("audio-volume-muted", Gtk::ICON_SIZE_MENU);
        return;
    }

    std::map<VolumeLevel, std::string> icon_name_from_state = {
        {VOLUME_LEVEL_MUTE, "audio-volume-muted"},
        {VOLUME_LEVEL_LOW, "audio-volume-low"},
        {VOLUME_LEVEL_MED, "audio-volume-medium"},
        {VOLUME_LEVEL_HIGH, "audio-volume-high"},
        {VOLUME_LEVEL_OOR, "audio-volume-muted"},
    };

    button->set_size_request(0, 0);
    main_image.set_from_icon_name(icon_name_from_state[current],
        Gtk::ICON_SIZE_MENU);
}

bool WayfireVolume::on_popover_timeout(int timer)
{
    button->get_popover()->popdown();
    return false;
}

void WayfireVolume::check_set_popover_timeout()
{
    popover_timeout.disconnect();
    if (this->button->is_popover_focused())
        return;

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

    if ((flags & VOLUME_FLAG_SHOW_POPOVER) &&
        !button->get_popover()->is_visible())
    {
        button->get_popover()->popup();
    }

    update_icon();
}

void WayfireVolume::on_volume_scroll(GdkEventScroll *event)
{
    int32_t current_volume = volume_scale.get_target_value();
    const int32_t adjustment_step = max_norm * INCREMENT_STEP_PC;

    /* Adjust volume on button scroll */
    if (event->direction == GDK_SCROLL_UP ||
        (event->direction == GDK_SCROLL_SMOOTH && event->delta_y < 0))
    {
        set_volume(std::min(current_volume + adjustment_step, (int32_t)max_norm));
    }

    if (event->direction == GDK_SCROLL_DOWN ||
        (event->direction == GDK_SCROLL_SMOOTH && event->delta_y > 0))
    {
        set_volume(std::max(current_volume - adjustment_step, 0));
    }

    button->grab_focus();
    check_set_popover_timeout();
}

void WayfireVolume::on_volume_button_press(GdkEventButton* event)
{
    if (event->button == 2 && event->type == GDK_BUTTON_PRESS) {
        /* Toggle mute on middle click */
        if (gvc_mixer_stream_get_is_muted(gvc_stream)) {
            gvc_mixer_stream_change_is_muted(gvc_stream, false);
            gvc_mixer_stream_set_is_muted(gvc_stream, false);
        } else {
            gvc_mixer_stream_change_is_muted(gvc_stream, true);
            gvc_mixer_stream_set_is_muted(gvc_stream, true);
        }
    }
}

void WayfireVolume::on_volume_changed_external()
{
    auto volume = gvc_mixer_stream_get_volume(gvc_stream);
    if (volume != (pa_volume_t)this->volume_scale.get_target_value())
    {
        /* When the volume changes externally, we want to temporarily show the
         * popover. However it shouldn't grab focus, because we're just displaying
         * a notification. */
        button->set_keyboard_interactive(false);
        set_volume(volume, VOLUME_FLAG_SHOW_POPOVER);
        button->set_keyboard_interactive(true);
    }

    check_set_popover_timeout();
}

static void notify_volume(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume *) user_data;
    wf_volume->on_volume_changed_external();
}

static void notify_is_muted(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume *) user_data;
    wf_volume->update_icon();
}

void WayfireVolume::disconnect_gvc_stream_signals()
{
    if (notify_volume_signal)
        g_signal_handler_disconnect(gvc_stream, notify_volume_signal);
    notify_volume_signal = 0;

    if (notify_is_muted_signal)
        g_signal_handler_disconnect(gvc_stream, notify_is_muted_signal);
    notify_is_muted_signal = 0;
}

void WayfireVolume::on_default_sink_changed()
{
    gvc_stream = gvc_mixer_control_get_default_sink(gvc_control);
    if (!gvc_stream) {
        printf("GVC: Failed to get default sink\n");
        return;
    }

    /* Reconnect signals to new sink */
    disconnect_gvc_stream_signals();
    notify_volume_signal = g_signal_connect (gvc_stream, "notify::volume",
        G_CALLBACK (notify_volume), this);
    notify_is_muted_signal = g_signal_connect (gvc_stream, "notify::is-muted",
        G_CALLBACK (notify_is_muted), this);

    /* Update the scale attributes */
    max_norm = gvc_mixer_control_get_vol_max_norm(gvc_control);
    volume_scale.set_range(0.0, max_norm);
    volume_scale.set_increments(max_norm * INCREMENT_STEP_PC,
        max_norm * INCREMENT_STEP_PC * 2);

    /* Finally, update the displayed volume. However, do not show the
     * popup */
    set_volume(gvc_mixer_stream_get_volume(gvc_stream), VOLUME_FLAG_NO_ACTION);
}


static void default_sink_changed (GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume *) user_data;
    wf_volume->on_default_sink_changed();
}

void WayfireVolume::on_volume_value_changed()
{
    /* User manually changed volume */
    button->grab_focus();
    set_volume(volume_scale.get_target_value());
}

void WayfireVolume::init(Gtk::HBox *container)
{
    volume_size.set_callback([=] () { update_icon(); });

    /* Setup button */
    button = std::make_unique<WayfireMenuButton> ("panel");
    auto style = button->get_style_context();
    style->context_save();
    style->set_state(Gtk::STATE_FLAG_NORMAL & ~Gtk::STATE_FLAG_PRELIGHT);
    button->reset_style();
    button->set_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK | Gdk::BUTTON_PRESS_MASK);
    button->signal_scroll_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_volume_scroll));
    button->signal_button_press_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_volume_button_press));
    button->property_scale_factor().signal_changed().connect(
        [=] () {update_icon(); });

    /* Setup popover */
    auto popover = button->get_popover();
    popover->add(volume_scale);
    popover->set_modal(false);

    volume_scale.set_draw_value(false);
    volume_scale.set_size_request(300, 0);
    volume_scale.set_user_changed_callback([=] () { on_volume_value_changed(); });

    volume_scale.signal_state_flags_changed().connect_notify(
        [=] (Gtk::StateFlags) { check_set_popover_timeout(); });

    /* Setup gvc control */
    gvc_control = gvc_mixer_control_new("Wayfire Volume Control");
    notify_default_sink_changed = g_signal_connect (gvc_control,
        "default-sink-changed", G_CALLBACK (default_sink_changed), this);
    gvc_mixer_control_open(gvc_control);

    /* Setup layout */
    container->pack_start(*button, false, false);
    button->add(main_image);
    button->show_all();
    button->get_popover()->show_all();
    button->get_popover()->hide(); // do not show the popover initially
}

WayfireVolume::~WayfireVolume()
{
    disconnect_gvc_stream_signals();

    if (notify_default_sink_changed)
        g_signal_handler_disconnect(gvc_control, notify_default_sink_changed);

    gvc_mixer_control_close(gvc_control);
    g_object_unref(gvc_control);

    popover_timeout.disconnect();
}
