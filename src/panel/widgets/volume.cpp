#include <iostream>
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

bool WayfireVolume::update_icon()
{
    int size = volume_size->as_int() / LAUNCHERS_ICON_SCALE;
    volume_level last, current;

    last = get_volume_level(last_volume);
    current = get_volume_level(current_volume);

    if (last == current)
        return true;

    button->set_size_request(size, 0);
    if (current == MUTE)
        main_image.set_from_icon_name("audio-volume-muted", Gtk::IconSize(size * main_image.get_scale_factor()));
    else if (current == LOW)
        main_image.set_from_icon_name("audio-volume-low", Gtk::IconSize(size * main_image.get_scale_factor()));
    else if (current == MED)
        main_image.set_from_icon_name("audio-volume-medium", Gtk::IconSize(size * main_image.get_scale_factor()));
    else if (current == HIGH)
        main_image.set_from_icon_name("audio-volume-high", Gtk::IconSize(size * main_image.get_scale_factor()));
    else
        printf("GVC: Volume out of range\n");

    return true;
}

void WayfireVolume::on_scroll(GdkEventScroll *event)
{
    if (event->direction == GDK_SCROLL_UP) {
        last_volume = current_volume;
        current_volume += inc;
        if (current_volume > max_norm)
            current_volume = max_norm;
        gvc_mixer_stream_set_volume(gvc_stream, current_volume);
        gvc_mixer_stream_push_volume(gvc_stream);
        update_icon();
    } else if (event->direction == GDK_SCROLL_DOWN) {
        last_volume = current_volume;
        current_volume -= inc;
        if (int32_t(current_volume) < 0)
            current_volume = 0;
        gvc_mixer_stream_set_volume(gvc_stream, current_volume);
        gvc_mixer_stream_push_volume(gvc_stream);
        update_icon();
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
    wf_volume->update_icon();
}

void WayfireVolume::init(Gtk::HBox *container, wayfire_config *config)
{
    auto config_section = config->get_section("panel");

    volume_size = config_section->get_option("launcher_size",
        std::to_string(DEFAULT_ICON_SIZE));
    volume_size_changed = [=] () { update_icon(); };
    volume_size->add_updated_handler(&volume_size_changed);

    button = std::unique_ptr<WayfireMenuButton> (new WayfireMenuButton(config));
    button->add(main_image);

    button->get_popover()->set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
    button->set_events(Gdk::SCROLL_MASK);
    button->signal_scroll_event().connect_notify(
        sigc::mem_fun(this, &WayfireVolume::on_scroll));

    if (!update_icon())
        return;

    button->property_scale_factor().signal_changed().connect(
        [=] () {update_icon(); });

    gvc_control = gvc_mixer_control_new("Wayfire Volume Control");
    g_signal_connect (gvc_control, "default-sink-changed",
        G_CALLBACK (default_sink_changed), this);
    bool gvc_open = gvc_mixer_control_open(gvc_control);
    if (gvc_open < 0) {
        printf("GVC: Failed to open mixer control\n");
        return;
    }

    container->pack_start(hbox, false, false);
    hbox.pack_start(*button, false, false);

    hbox.show();
    main_image.show();
    button->show();
}

void WayfireVolume::focus_lost()
{
    button->set_active(false);
}

WayfireVolume::~WayfireVolume()
{
    volume_size->rem_updated_handler(&volume_size_changed);
}
