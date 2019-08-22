#include <iostream>
#include <glibmm.h>
#include "volume.hpp"
#include "launchers.hpp"
#include "config.hpp"
#include "gtk-utils.hpp"

volume_level
WayfireVolume::get_volume_level(pa_volume_t v)
{
    if (v == 0)
        return MUTE;
    else if (v > 0 && v <= (max_norm / 3))
        return LOW;
    else if (v > (max_norm / 3) && v <= ((max_norm / 3) * 2))
        return MED;
    else if (v > ((max_norm / 3) * 2) && v <= max_norm)
        return HIGH;

    return OOR;
}

void
WayfireVolume::update_icon()
{
    volume_level last, current;

    last = get_volume_level(last_volume);
    current = get_volume_level(current_volume);

    last_volume = current_volume;

    if (last == current)
        return;

    button->set_size_request(0, 0);
    if (current == MUTE) {
        main_image.set_from_icon_name("audio-volume-muted", Gtk::ICON_SIZE_MENU);
    } else if (current == LOW) {
        main_image.set_from_icon_name("audio-volume-low",  Gtk::ICON_SIZE_MENU);
    } else if (current == MED) {
        main_image.set_from_icon_name("audio-volume-medium", Gtk::ICON_SIZE_MENU);
    } else if (current == HIGH) {
        main_image.set_from_icon_name("audio-volume-high", Gtk::ICON_SIZE_MENU);
    } else {
        gvc_mixer_stream_set_volume(gvc_stream, 0.0);
        main_image.set_from_icon_name("audio-volume-muted", Gtk::ICON_SIZE_MENU);
    }
}

bool
WayfireVolume::on_popover_timeout(int timer)
{
    button->get_popover()->hide();
    return false;
}

void
WayfireVolume::update_volume(pa_volume_t volume)
{
    current_volume = volume;

    if (current_volume > max_norm)
        current_volume = max_norm;
    else if (int32_t(current_volume) < 0)
        current_volume = 0;

    volume_changed_signal.block();
    volume_scale.set_value(current_volume);
    volume_changed_signal.unblock();
    volume_scale.queue_draw();

    gvc_mixer_stream_set_volume(gvc_stream, current_volume);
    gvc_mixer_stream_push_volume(gvc_stream);

    button->get_popover()->show_all();
    conn.disconnect();
    conn = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
              &WayfireVolume::on_popover_timeout), 0), 2500);

    update_icon();
}

void
WayfireVolume::on_scroll(GdkEventScroll *event)
{
    if (event->direction == GDK_SCROLL_SMOOTH) {
        if (event->delta_y > 0)
            update_volume(current_volume - inc);
        else if (event->delta_y < 0)
            update_volume(current_volume + inc);
    }
}

static void
default_sink_changed (GvcMixerControl *gvc_control,
                      guint            id,
                      gpointer         user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume *) user_data;

    wf_volume->gvc_stream = gvc_mixer_control_get_default_sink(gvc_control);
    if (!wf_volume->gvc_stream) {
        printf("GVC: Failed to get default sink\n");
        return;
    }
    wf_volume->max_norm = gvc_mixer_control_get_vol_max_norm(gvc_control);
    wf_volume->inc = wf_volume->max_norm / 20;

    wf_volume->current_volume = gvc_mixer_stream_get_volume(wf_volume->gvc_stream);

    wf_volume->volume_scale.set_increments(wf_volume->inc, wf_volume->inc);
    wf_volume->volume_scale.set_range(0.0, wf_volume->max_norm);
    wf_volume->volume_scale.set_value(wf_volume->current_volume);

    wf_volume->update_icon();
}

void
WayfireVolume::on_volume_value_changed()
{
    update_volume(volume_scale.get_value());
    conn.disconnect();
    conn = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
              &WayfireVolume::on_popover_timeout), 0), 2500);
}

void
WayfireVolume::init(Gtk::HBox *container, wayfire_config *config)
{
    auto config_section = config->get_section("panel");

    volume_size = config_section->get_option("launcher_size",
        std::to_string(DEFAULT_ICON_SIZE));
    volume_size_changed = [=] () { update_icon(); };
    volume_size->add_updated_handler(&volume_size_changed);

    button = std::unique_ptr<WayfireMenuButton> (new WayfireMenuButton(config));
    button->add(main_image);
    auto style = button->get_style_context();
    style->context_save();
    style->set_state(Gtk::STATE_FLAG_NORMAL & ~Gtk::STATE_FLAG_PRELIGHT);
    button->reset_style();
    auto popover = button->get_popover();
    popover->set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
    popover->add(volume_scale);
    popover->set_modal(false);
    button->set_events(Gdk::SMOOTH_SCROLL_MASK);
    button->signal_scroll_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_scroll));

    volume_scale.set_draw_value(false);
    volume_scale.set_size_request(300, 0);
    volume_changed_signal = volume_scale.signal_value_changed().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_volume_value_changed));

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

void
WayfireVolume::focus_lost()
{
    button->set_active(false);
}

WayfireVolume::~WayfireVolume()
{
    volume_size->rem_updated_handler(&volume_size_changed);
}
