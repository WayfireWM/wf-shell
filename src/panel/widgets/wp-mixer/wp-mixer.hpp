#pragma once

#include <gtkmm.h>
#include <wp/proxy-interfaces.h>
extern "C" {
#include <wp/wp.h>
}
#include <map>

#include "widget.hpp"
#include "wf-popover.hpp"
#include "wf-wp-control.hpp"
#include "wp-common.hpp"

enum class QuickTargetChoice // config
{
    LAST_CHANGE,
    DEFAULT_SINK,
    DEFAULT_SOURCE,
};

class WfWpControl;

class WayfireWpMixer : public WayfireWidget
{
  private:
    void init(Gtk::Box *container) override;

    Gtk::Image main_image;

    WfOption<double> timeout{"panel/wp_display_timeout"};
    WfOption<int> icon_size{"panel/wp_icon_size"};

    void on_volume_value_changed();
    bool on_popover_timeout(int timer);

    // signals and gestures for the configurable actions on click
    gulong notify_volume_signal   = 0;
    gulong notify_is_muted_signal = 0;
    gulong notify_default_sink_changed = 0;
    sigc::connection popover_timeout;
    sigc::connection volume_changed_signal;
    bool gestures_initialised = false;
    std::shared_ptr<Gtk::GestureClick> left_click_gesture, middle_click_gesture, right_click_gesture;
    sigc::connection left_conn, middle_conn, right_conn;

    // widgets for the mixer itself
    Gtk::Label output_label, input_label, streams_label;
    Gtk::Separator out_in_wall, in_streams_wall, out_sep, in_sep, streams_sep;

    void reload_config();

  public:

    WfOption<double> scroll_sensitivity{"panel/wp_scroll_sensitivity"};
    WfOption<bool> invert_scroll{"panel/wp_invert_scroll"};
    WfOption<bool> popup_on_change{"panel/wp_popup_on_change"};

    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Popover *popover;

    /*
     * the "quick_target" is the representation of the audio channel that shows it’s volume
     * level on the widget icon and is concerned by the quick actions.
     * configured by panel/wp_quick_target_choice. idea: add pinning?
     */
    QuickTargetChoice quick_target_choice;
    std::unique_ptr<WfWpControl> quick_target;
    void set_quick_target_from(WfWpControl *from);

    Gtk::Box master_box, sinks_box, sources_box, streams_box;
    // idea: add a category for stuff that listens to an audio source

    std::map<WpPipewireObject*, std::unique_ptr<WfWpControl>> objects_to_controls;

    /** Update the icon based on volume and muted state of the quick_target widget */
    void update_icon();

    /**
     * Check whether the popover should be auto-hidden, and if yes, start a timer to hide it
     */
    void check_set_popover_timeout();

    // cancel popover self-hiding. mostly used after user interactions to an externally opened popover
    void cancel_popover_timeout();

    void handle_config_reload() override;

    virtual ~WayfireWpMixer();
};
