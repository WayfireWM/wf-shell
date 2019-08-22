#ifndef WIDGETS_VOLUME_HPP
#define WIDGETS_VOLUME_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <gtkmm/image.h>
#include <pulse/pulseaudio.h>
#include "gvc-mixer-control.h"

class WayfireVolume : public WayfireWidget
{
    Gtk::HBox hbox;
    Gtk::Image main_image;
    std::unique_ptr<WayfireMenuButton> button;

    bool update_icon();

    wf_option panel_position;
    wf_option_callback panel_position_changed;

    wf_option volume_size;
    wf_option_callback volume_size_changed;

    void on_scroll(GdkEventScroll *event);

    GvcMixerControl *gvc_control;

    public:
    void init(Gtk::HBox *container, wayfire_config *config) override;
    virtual ~WayfireVolume();
    void focus_lost() override;
    GvcMixerStream *gvc_stream;
};

#endif /* end of include guard: WIDGETS_VOLUME_HPP */
