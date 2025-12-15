#pragma once

#include "../widget.hpp"
#include <gtkmm/image.h>
#include "../../util/animated-scale.hpp"
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>
#include <wayfire/util/duration.hpp>

class WayfireVolume : public WayfireWidget
{
    Gtk::Image main_image;
    WayfireAnimatedScale volume_scale;
    Gtk::Button button;
    Gtk::Popover popover;

    WfOption<double> timeout{"panel/volume_display_timeout"};
    WfOption<double> scroll_sensitivity{"panel/volume_scroll_sensitivity"};

    // void on_volume_scroll(GdkEventScroll *event);
    // void on_volume_button_press(GdkEventButton *event);
    void on_volume_value_changed();
    bool on_popover_timeout(int timer);

    GvcMixerControl *gvc_control;
    GvcMixerStream *gvc_stream = NULL;
    gdouble max_norm; // maximal volume for current stream

    gulong notify_volume_signal   = 0;
    gulong notify_is_muted_signal = 0;
    gulong notify_default_sink_changed = 0;
    sigc::connection popover_timeout;
    std::vector<sigc::connection> signals;
    void disconnect_gvc_stream_signals();

    enum set_volume_flags_t
    {
        /* Neither show popover nor update volume */
        VOLUME_FLAG_NO_ACTION    = 0,
        /* Show volume popover */
        VOLUME_FLAG_SHOW_POPOVER = 1,
        /* Update real volume with GVC */
        VOLUME_FLAG_UPDATE_GVC   = 2,
        /* Both of the above */
        VOLUME_FLAG_FULL         = 3,
    };

    /**
     * Set the current volume level to volume_level. This updates both the popover scale and the real
     * pulseaudio volume, depending on the passed flags.
     *
     * Precondition: volume_level should be between 0 and max_norm
     */
    void set_volume(pa_volume_t volume_level,
        set_volume_flags_t flags = VOLUME_FLAG_FULL);

  public:
    void init(Gtk::Box *container) override;
    virtual ~WayfireVolume();

    /** Update the icon based on volume and muted state */
    void update_icon();

    /** Called when the volume changed from outside of the widget */
    void on_volume_changed_external();

    /** Called when the default sink changes */
    void on_default_sink_changed();

    /**
     * Check whether the popover should be auto-hidden, and if yes, start a timer to hide it
     */
    void check_set_popover_timeout();
};
