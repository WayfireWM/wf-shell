#pragma once

#include <gtkmm.h>
#include <wp/proxy-interfaces.h>
extern "C" {
#include <wp/wp.h>
}
#include <map>

#include "widget.hpp"
#include "wf-popover.hpp"
#include "mixer-control.hpp"
#include "wp-common.hpp"

enum class QuickTargetChoice // config
{
    LAST_CHANGE,
    DEFAULT_SINK,
    DEFAULT_SOURCE,
};

class MixerControl;

class WayfireMixer : public WayfireWidget
{
  private:
    void init(Gtk::Box *container) override;

    Gtk::Image main_image;

    WfOption<int> spacing{"panel/mixer_spacing"};
    WfOption<std::string> layout{"panel/mixer_layout"};

    void on_volume_value_changed();

    // signals and gestures for the configurable actions on click
    gulong notify_volume_signal   = 0;
    gulong notify_is_muted_signal = 0;
    gulong notify_default_sink_changed = 0;
    sigc::connection volume_changed_signal, left_conn, middle_conn, right_conn, scroll_conn;
    std::shared_ptr<Gtk::GestureClick> left_click_gesture, middle_click_gesture, right_click_gesture;

    // widgets for the mixer itself
    Gtk::Label output_label, input_label, streams_label;
    Gtk::Separator out_in_wall, in_streams_wall, out_sep, in_sep, streams_sep;

    void reload_config();

  public:

    WfOption<double> scroll_sensitivity{"panel/mixer_scroll_sensitivity"};
    WfOption<bool> invert_scroll{"panel/mixer_invert_scroll"};
    WfOption<bool> popup_on_change{"panel/mixer_popup_on_change"};
    WfOption<double> timeout{"panel/mixer_popup_timeout"};

    std::unique_ptr<WayfireMenuWidget> button;

    /*
     * the "quick_target" is the representation of the audio channel that shows it’s volume
     * level on the widget icon and is concerned by the quick actions.
     * configured by panel/mixer_quick_target_choice. idea: add pinning?
     */
    QuickTargetChoice quick_target_choice;
    std::unique_ptr<MixerControl> quick_target;
    void set_quick_target_from(MixerControl *from);

    Gtk::Box master_box, sinks_box, sources_box, streams_box;
    // idea: add a category for stuff that listens to an audio source

    std::map<WpPipewireObject*, std::unique_ptr<MixerControl>> objects_to_controls;

    /** Update the icon based on volume and muted state of the quick_target widget */
    void update_icon();

    void handle_config_reload() override;

    virtual ~WayfireMixer();
};
