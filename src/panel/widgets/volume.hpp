#ifndef WIDGETS_VOLUME_HPP
#define WIDGETS_VOLUME_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <gtkmm/image.h>
#include <gtkmm/scale.h>
#include <pulse/pulseaudio.h>
#include "gvc-mixer-control.h"


enum volume_level {
    VOLUME_LEVEL_MUTE = 0,
    VOLUME_LEVEL_LOW,
    VOLUME_LEVEL_MED,
    VOLUME_LEVEL_HIGH,
    VOLUME_LEVEL_OOR /* Out of range */
};

class WayfireVolume : public WayfireWidget
{
    Gtk::HBox hbox;
    Gtk::Image main_image;
    wf_option panel_position;
    wf_option_callback panel_position_changed;

    wf_option volume_size;
    wf_option_callback volume_size_changed;

    void reset_popover_timeout();
    bool timeout_was_enabled = false;
    wf_option timeout;

    void on_volume_scroll(GdkEventScroll *event);
    void on_scale_button_press(GdkEventButton *event);
    void on_scale_button_release(GdkEventButton *event);
    void on_volume_button_press(GdkEventButton *event);
    void on_popover_button_press(GdkEventButton *event);
    void on_popover_hide();

    bool scale_pressed = false;

    GvcMixerControl *gvc_control;

    volume_level get_volume_level(pa_volume_t v);
    void update_volume(pa_volume_t volume);
    void on_volume_value_changed();
    bool on_popover_timeout(int timer);

    public:
    void init(Gtk::HBox *container, wayfire_config *config) override;
    virtual ~WayfireVolume();
    void update_icon();

    GvcMixerStream *gvc_stream;
    gulong notify_volume_signal, notify_is_muted_signal;
    sigc::connection popover_timeout, volume_changed_signal;
    int32_t current_volume, last_volume;
    Gtk::Scale volume_scale;
    gdouble max_norm, inc;
    std::unique_ptr<WayfireMenuButton> button;
};

#endif /* end of include guard: WIDGETS_VOLUME_HPP */
