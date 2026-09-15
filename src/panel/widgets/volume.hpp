#pragma once

#include <gtkmm/button.h>
#include <gtkmm/image.h>

#include <wayfire/util/duration.hpp>

#include "../widget.hpp"
#include "wf-popover.hpp"
#include "widget-utils/pulseaudio/control.hpp"

class WayfireVolume;

class VolCtrl : public GvcControl
{
  public:
    VolCtrl(WayfireVolume *widget, StreamRole role, std::string section);
    void on_volume(pa_volume_t newstate) override;
    WayfireVolume *widget;
    WfOption<double> timeout{"panel/volume_display_timeout"};
};

class WayfireVolume : public WayfireWidget
{
    Gtk::Image main_image;
    VolCtrl control;

    WfOption<double> scroll_sensitivity{"panel/volume_scroll_sensitivity"};

    std::vector<sigc::connection> signals;

  public:
    WayfireVolume(StreamRole role);
    void init(Gtk::Box *container) override;
    std::unique_ptr<WayfireMenuWidget> button;
    virtual ~WayfireVolume();
};
