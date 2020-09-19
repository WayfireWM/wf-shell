#ifndef WIDGETS_VOLUME_HPP
#define WIDGETS_VOLUME_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <gtkmm/image.h>
#include <gtkmm/scale.h>
#include <pulse/pulseaudio.h>
#include "gvc-mixer-control.h"
#include <wayfire/util/duration.hpp>

/**
 * A custom scale which animates transitions when its value is
 * changed programatically.
 */
class WayfireVolumeScale : public Gtk::Scale
{
    wf::animation::simple_animation_t current_volume{wf::create_option(200)};
    sigc::connection value_changed;
    std::function<void()> user_changed_callback;

  public:
    WayfireVolumeScale();

    /* Gets the current target value */
    double get_target_value() const;
    /* Set a target value to animate towards */
    void set_target_value(double value);
    /** Set the callback when the user changes the scale value */
    void set_user_changed_callback(std::function<void()> callback);
};

class WayfireVolume : public WayfireWidget
{
    Gtk::Image main_image;
    WayfireVolumeScale volume_scale;
    std::unique_ptr<WayfireMenuButton> button;

    WfOption<int> volume_size{"panel/launchers_size"};
    WfOption<double> timeout{"panel/volume_display_timeout"};

    void on_volume_scroll(GdkEventScroll *event);
    void on_volume_button_press(GdkEventButton *event);
    void on_volume_value_changed();
    bool on_popover_timeout(int timer);

    GvcMixerControl *gvc_control;
    GvcMixerStream *gvc_stream = NULL;
    gdouble max_norm; // maximal volume for current stream

    gulong notify_volume_signal = 0;
    gulong notify_is_muted_signal = 0;
    gulong notify_default_sink_changed = 0;
    sigc::connection popover_timeout;
    sigc::connection volume_changed_signal;
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
     * Set the current volume level to volume_level.
     * This updates both the popover scale and the real pulseaudio volume,
     * depending on the passed flags.
     *
     * Precondition: volume_level should be between 0 and max_norm
     */
    void set_volume(pa_volume_t volume_level,
        set_volume_flags_t flags = VOLUME_FLAG_FULL);


  public:
    void init(Gtk::HBox *container) override;
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
