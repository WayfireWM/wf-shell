#ifndef WIDGETS_VOLUME_HPP
#define WIDGETS_VOLUME_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <gtkmm/image.h>
#include <gtkmm/scale.h>
#include <pulse/pulseaudio.h>
#include "gvc-mixer-control.h"

class WayfireVolume : public WayfireWidget
{
    Gtk::Image main_image;
    Gtk::Scale volume_scale;
    std::unique_ptr<WayfireMenuButton> button;

    wf_option volume_size;
    wf_option_callback volume_size_changed;
    wf_option timeout;

    void on_volume_scroll(GdkEventScroll *event);
    void on_volume_button_press(GdkEventButton *event);
    void on_volume_value_changed();
    bool on_popover_timeout(int timer);

    GvcMixerControl *gvc_control;
    GvcMixerStream *gvc_stream = NULL;
    gdouble max_norm; // maximal volume for current stream

    gulong notify_volume_signal = 0;
    gulong notify_is_muted_signal = 0;
    sigc::connection popover_timeout;
    sigc::connection volume_changed_signal;

    /**
     * Set the current volume level to volume_level.
     * This updates both the popover scale and the real pulseaudio volume,
     * if such an update is necessary.
     *
     * Precondition: volume_level should be between 0 and max_norm
     */
    void set_volume(pa_volume_t volume_level, bool show_popover = true);


  public:
    void init(Gtk::HBox *container, wayfire_config *config) override;
    virtual ~WayfireVolume();

    /** Update the icon based on volume and muted state */
    void update_icon();

    /** Called when the volume changed from outside of the widget */
    void on_volume_changed_external();

    /** Called when the default sink changes */
    void on_default_sink_changed();

    /**
     * Check whether the popover should be auto-hidden, and if yes, start
     * a timer to hide it
     */
    void check_set_popover_timeout();
};

#endif /* end of include guard: WIDGETS_VOLUME_HPP */
