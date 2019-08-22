#include <iostream>
#include <glibmm.h>
#include "volume.hpp"
#include "launchers.hpp"
#include "config.hpp"
#include "gtk-utils.hpp"

volume_level WayfireVolume::get_volume_level(pa_volume_t v)
{
    auto third = max_norm / 3;
    if (v == 0)
        return VOLUME_LEVEL_MUTE;
    else if (v > 0 && v <= third)
        return VOLUME_LEVEL_LOW;
    else if (v > third && v <= (third * 2))
        return VOLUME_LEVEL_MED;
    else if (v > (third * 2) && v <= max_norm)
        return VOLUME_LEVEL_HIGH;

    return VOLUME_LEVEL_OOR;
}

void WayfireVolume::update_icon()
{
    volume_level last = get_volume_level(last_volume);
    volume_level current = get_volume_level(current_volume);

    last_volume = current_volume;

    if (gvc_mixer_stream_get_is_muted(gvc_stream)) {
        main_image.set_from_icon_name("audio-volume-muted", Gtk::ICON_SIZE_MENU);
        return;
    }

    if (last == current)
        return;

    button->set_size_request(0, 0);
    if (current == VOLUME_LEVEL_MUTE) {
        main_image.set_from_icon_name("audio-volume-muted", Gtk::ICON_SIZE_MENU);
    } else if (current == VOLUME_LEVEL_LOW) {
        main_image.set_from_icon_name("audio-volume-low",  Gtk::ICON_SIZE_MENU);
    } else if (current == VOLUME_LEVEL_MED) {
        main_image.set_from_icon_name("audio-volume-medium", Gtk::ICON_SIZE_MENU);
    } else if (current == VOLUME_LEVEL_HIGH) {
        main_image.set_from_icon_name("audio-volume-high", Gtk::ICON_SIZE_MENU);
    } else {
        gvc_mixer_stream_set_volume(gvc_stream, 0.0);
        main_image.set_from_icon_name("audio-volume-muted", Gtk::ICON_SIZE_MENU);
    }
}

bool WayfireVolume::on_popover_timeout(int timer)
{
    button->get_popover()->popdown();
    return false;
}

void WayfireVolume::reset_popover_timeout()
{
    if (scale_pressed || button->get_state_flags() & Gtk::STATE_FLAG_SELECTED)
        return;

    popover_timeout.disconnect();
    popover_timeout = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
        &WayfireVolume::on_popover_timeout), 0), timeout->as_double() * 1000);
}

void WayfireVolume::update_volume(pa_volume_t volume)
{
    current_volume = volume;

    if (current_volume > max_norm)
        current_volume = max_norm;
    else if (current_volume < 0)
        current_volume = 0;

    volume_changed_signal.block();
    volume_scale.set_value(current_volume);
    volume_changed_signal.unblock();
    volume_scale.queue_draw();

    gvc_mixer_stream_set_volume(gvc_stream, current_volume);
    gvc_mixer_stream_push_volume(gvc_stream);

    button->get_popover()->popup();
    reset_popover_timeout();

    update_icon();
}

void WayfireVolume::on_volume_scroll(GdkEventScroll *event)
{
    /* Adjust volume on button scroll */
    if (event->direction == GDK_SCROLL_UP) {
        update_volume(current_volume + inc);
    } else if (event->direction == GDK_SCROLL_DOWN) {
        update_volume(current_volume - inc);
    } else if (event->direction == GDK_SCROLL_SMOOTH) {
        if (event->delta_y > 0)
            update_volume(current_volume - inc);
        else if (event->delta_y < 0)
            update_volume(current_volume + inc);
    }
}

void WayfireVolume::on_volume_button_press(GdkEventButton* event)
{
    if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
        if (button->get_popover()->is_visible())
            button->get_popover()->popdown();
    } else if (event->button == 2 && event->type == GDK_BUTTON_PRESS) {
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

void WayfireVolume::on_scale_button_press(GdkEventButton* event)
{
    timeout_was_enabled = !popover_timeout.empty();
    popover_timeout.disconnect();
    scale_pressed = true;
}

void WayfireVolume::on_scale_button_release(GdkEventButton* event)
{
    if (timeout_was_enabled)
        popover_timeout = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
            &WayfireVolume::on_popover_timeout), 0), timeout->as_double() * 1000);
    scale_pressed = false;
}

void WayfireVolume::on_popover_button_press(GdkEventButton* event)
{
    button->get_popover()->popdown();
}

static void notify_volume (GvcMixerControl *gvc_control,
                guint            id,
                gpointer         user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume *) user_data;

    wf_volume->button->set_keyboard_interactive(false);
    wf_volume->current_volume = gvc_mixer_stream_get_volume(wf_volume->gvc_stream);
    wf_volume->volume_scale.set_value(wf_volume->current_volume);
}

static void notify_is_muted (GvcMixerControl *gvc_control,
                guint            id,
                gpointer         user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume *) user_data;

    wf_volume->last_volume = -1;

    wf_volume->update_icon();
}

static void default_sink_changed (GvcMixerControl *gvc_control,
                      guint            id,
                      gpointer         user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume *) user_data;

    wf_volume->gvc_stream = gvc_mixer_control_get_default_sink(gvc_control);
    if (!wf_volume->gvc_stream) {
        printf("GVC: Failed to get default sink\n");
        return;
    }

    if (wf_volume->notify_volume_signal)
        g_signal_handler_disconnect(wf_volume->gvc_stream, wf_volume->notify_volume_signal);

    wf_volume->notify_volume_signal = g_signal_connect (wf_volume->gvc_stream, "notify::volume",
        G_CALLBACK (notify_volume), user_data);

    if (wf_volume->notify_is_muted_signal)
        g_signal_handler_disconnect(wf_volume->gvc_stream, wf_volume->notify_is_muted_signal);

    wf_volume->notify_is_muted_signal = g_signal_connect (wf_volume->gvc_stream, "notify::is-muted",
        G_CALLBACK (notify_is_muted), user_data);

    wf_volume->max_norm = gvc_mixer_control_get_vol_max_norm(gvc_control);
    wf_volume->inc = wf_volume->max_norm / 20;

    wf_volume->current_volume = gvc_mixer_stream_get_volume(wf_volume->gvc_stream);

    wf_volume->volume_scale.set_increments(wf_volume->inc, wf_volume->inc);
    wf_volume->volume_scale.set_range(0.0, wf_volume->max_norm);
    wf_volume->volume_scale.set_value(wf_volume->current_volume);

    wf_volume->update_icon();

    wf_volume->volume_changed_signal.unblock();
}

void WayfireVolume::on_volume_value_changed()
{
    update_volume(volume_scale.get_value());
}

void WayfireVolume::init(Gtk::HBox *container, wayfire_config *config)
{
    auto config_section = config->get_section("panel");
    timeout = config_section->get_option("volume_display_timeout", "2.5");

    volume_size = config_section->get_option("launcher_size",
        std::to_string(DEFAULT_ICON_SIZE));
    volume_size_changed = [=] () { update_icon(); };
    volume_size->add_updated_handler(&volume_size_changed);

    button = std::make_unique<WayfireMenuButton> (PANEL_POSITION_OPT(config));
    button->add(main_image);
    auto style = button->get_style_context();
    style->context_save();
    style->set_state(Gtk::STATE_FLAG_NORMAL & ~Gtk::STATE_FLAG_PRELIGHT);
    button->reset_style();
    auto popover = button->get_popover();
    popover->add(volume_scale);
    popover->set_modal(false);
    popover->set_events(Gdk::BUTTON_PRESS_MASK);
    popover->signal_button_press_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_popover_button_press));
    button->set_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK | Gdk::BUTTON_PRESS_MASK);
    button->signal_scroll_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_volume_scroll));
    button->signal_button_press_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_volume_button_press));

    volume_scale.set_draw_value(false);
    volume_scale.set_size_request(300, 0);
    volume_changed_signal = volume_scale.signal_value_changed().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_volume_value_changed));
    volume_scale.set_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
    volume_scale.signal_button_press_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_scale_button_press));
    volume_scale.signal_button_release_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_scale_button_release));
    volume_changed_signal.block();

    last_volume = -1;

    button->property_scale_factor().signal_changed().connect(
        [=] () {update_icon(); });

    gvc_control = gvc_mixer_control_new("Wayfire Volume Control");

    g_signal_connect (gvc_control, "default-sink-changed",
        G_CALLBACK (default_sink_changed), this);

    gvc_mixer_control_open(gvc_control);

    container->pack_start(hbox, false, false);
    hbox.pack_start(*button, false, false);

    volume_scale.show();
    hbox.show();
    main_image.show();
    button->show();
}

WayfireVolume::~WayfireVolume()
{
    volume_size->rem_updated_handler(&volume_size_changed);
}
