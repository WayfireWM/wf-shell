#pragma once

#include <gtkmm.h>

#include "mixer.hpp"
#include "animated-scale.hpp"
#include "icon-select.hpp"

class WayfireMixer;

class MixerControl : public Gtk::Grid
{
    // Custom grid to facilitate handling

  protected:
    WayfireAnimatedScale scale;
    Gtk::Image volume_icon;
    sigc::connection mute_conn;
    WayfireMixer *parent;
    std::shared_ptr<Gtk::GestureClick> middle_click_mute, right_click_mute;
    sigc::connection middle_conn, right_conn;
    std::vector<sigc::connection> signals;
    void update_gestures();
    virtual void update_icons_pos();
    std::map<double, std::vector<std::string>> icon_set;
    WfOption<int> slider_length{"panel/mixer_slider_length"};

  public:
    MixerControl(WpPipewireObject *obj, WayfireMixer *parent_widget, const std::map<double,
        std::vector<std::string>> icon_set = volume_icons);
    ~MixerControl();
    virtual void init();

    WpPipewireObject *object;
    Gtk::Label label;
    Gtk::ToggleButton button;
    std::string name;
    void set_btn_status_no_callbk(bool state);
    void set_scale_target_value(double volume);
    double get_scale_target_value();
    void update_icon();
    std::string get_icon_name();
    // used to mark the control as the source of changes and stop useless/counterproductive updates
    bool ignore = false; // set when volume changes because of it to ignore refresh of ui

    virtual void handle_config_reload();

    std::unique_ptr<MixerControl> copy();
};

// idea: would be neat to have a MixerControlStream class that presents a dropdown to select which sink a
// stream goes to

// sinks and sources: a control with a button to set itself as default for it’s category
class MixerControlDevice : public MixerControl
{
  private:

    sigc::connection def_conn;
    Gtk::Image is_def_icon;
    void update_icons_pos();

  public:
    MixerControlDevice(WpPipewireObject *obj, WayfireMixer *parent_widget, const std::map<double,
        std::vector<std::string>> icon_set = volume_icons) : MixerControl(obj, parent_widget, icon_set)
    {}
    ~MixerControlDevice();
    void init();

    Gtk::ToggleButton default_btn;
    void set_def_status_no_callbk(bool state);

    void handle_config_reload();
};
