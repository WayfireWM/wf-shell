#pragma once

#include <gtkmm.h>

#include "wireplumber.hpp"
#include "animated-scale.hpp"

class WayfireWpMixer;

class WfWpControl : public Gtk::Grid
{
    // Custom grid to facilitate handling

  protected:
    WayfireAnimatedScale scale;
    Gtk::Label label;
    Gtk::Image volume_icon;
    sigc::connection mute_conn;
    WayfireWpMixer *parent;
    std::shared_ptr<Gtk::GestureClick> middle_click_mute, right_click_mute;
    sigc::connection middle_conn, right_conn;
    bool gestures_initialised = false;
    void update_gestures();
    WfOption<int> slider_length{"panel/wp_slider_length"};

  public:
    WfWpControl(WpPipewireObject *obj, WayfireWpMixer *parent_widget);
    WpPipewireObject *object;
    Gtk::ToggleButton button;
    void set_btn_status_no_callbk(bool state);
    void set_scale_target_value(double volume);
    double get_scale_target_value();
    void update_icon();
    // used to mark the control as the source of changes and determine what should be done
    enum ignore_type
    {
        DONT_IGNORE, // not the source, normal treatment
        IGNORE_ALL, // the source, no need to touch it
        ONLY_UPDATE, // the source, but the visuals need to be updated. used for scrolling.
    };

    ignore_type ignore; // set when volume changes because of it to ignore refresh of ui

    void handle_config_reload();

    std::unique_ptr<WfWpControl> copy();
};

// idea: would be neat to have a WfWpControlStream class that presents a dropdown to select which sink a
// stream goes to

// sinks and sources: a control with a button to set itself as default for it’s category
class WfWpControlDevice : public WfWpControl
{
  private:
    sigc::connection def_conn;
    Gtk::Image is_def_icon;

  public:
    WfWpControlDevice(WpPipewireObject *obj, WayfireWpMixer *parent_widget);
    Gtk::ToggleButton default_btn;
    void set_def_status_no_callbk(bool state);
};
