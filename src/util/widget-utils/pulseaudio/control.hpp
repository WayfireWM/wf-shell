#pragma once

#include <gtkmm/box.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/image.h>
#include <sigc++/connection.h>

#include "animated-scale.hpp"
#include "gvc-proxy.hpp"

enum class StreamRole;

class GvcControl : public Gtk::Box
{
  public:
    GvcControl(StreamRole role, std::string section);
    virtual void on_mute(bool newstate);
    virtual void on_volume(pa_volume_t newstate);
    void set_btn_status_no_callbk(bool state);
    void set_scale_target_value(double volume);
    double get_scale_target_value();
    void update_icon();
    void set_orientation(Gtk::Orientation orientation);
    Gtk::Image *managed_icon = nullptr; // icon given by something else that we manage
    StreamRole get_role();
    ~GvcControl();

  private:
    StreamRole role;
    std::map<double, std::vector<std::string>> icon_map;
    WayfireAnimatedScale scale;
    Gtk::ToggleButton mute_button;
    Gtk::Image icon;
    sigc::connection mute_conn;
    std::vector<sigc::connection> signals;
    WfOption<double> scroll_sensitivity;
};
