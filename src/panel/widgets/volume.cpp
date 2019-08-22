#include <iostream>
#include "volume.hpp"
#include "launchers.hpp"
#include "config.hpp"
#include "gtk-utils.hpp"


bool WayfireVolume::update_icon()
{
    int size = volume_size->as_int() / LAUNCHERS_ICON_SCALE;

    button->set_size_request(size, 0);
    main_image.set_from_icon_name("audio-volume-high", Gtk::IconSize(size * main_image.get_scale_factor()));

    return true;
}

void WayfireVolume::on_scroll(GdkEventScroll *event)
{
    printf("Got event!\n");
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
    bool gvc_open = gvc_mixer_control_open(gvc_control);
    if (!gvc_open) {
        printf("GVC: Failed to open control\n");
        return;
    }

    gvc_stream = gvc_mixer_control_get_default_sink(gvc_control);
    if (!gvc_stream) {
        printf("GVC: Failed to get default sink\n");
        return;
    }

    pa_volume_t volume = gvc_mixer_stream_get_volume(gvc_stream);
    printf("GVC: volume: %d\n", volume);

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
