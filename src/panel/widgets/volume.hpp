#ifndef WIDGETS_VOLUME_HPP
#define WIDGETS_VOLUME_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <gtkmm/image.h>
#include <pulse/pulseaudio.h>
#include "gvc-mixer-control.h"


enum volume_level {
    MUTE = 0,
    LOW,
    MED,
    HIGH,
    OOR
};

class WayfireVolume : public WayfireWidget
{
    Gtk::HBox hbox;
    Gtk::Image main_image;
    std::unique_ptr<WayfireMenuButton> button;

    wf_option panel_position;
    wf_option_callback panel_position_changed;

    wf_option volume_size;
    wf_option_callback volume_size_changed;

    void on_scroll(GdkEventScroll *event);

    GvcMixerControl *gvc_control;

    volume_level get_volume_level(pa_volume_t v);
    void update_volume(int direction);

    public:
    void init(Gtk::HBox *container, wayfire_config *config) override;
    virtual ~WayfireVolume();
    void focus_lost() override;
    void update_icon();

    GvcMixerStream *gvc_stream;
    pa_volume_t current_volume, last_volume;
    gdouble max_norm, inc;
};

#endif /* end of include guard: WIDGETS_VOLUME_HPP */
