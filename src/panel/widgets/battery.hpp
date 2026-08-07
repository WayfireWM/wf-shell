#pragma once

#include <sigc++/connection.h>

#include "widget.hpp"
#include "widget-utils/battery.hpp"

class WayfireBatteryInfo : public WayfireWidget
{
    ShellBattery battery;

    void update_layout();
    void handle_config_reload();

  public:
    WayfireBatteryInfo() : battery("panel")
    {}
    virtual void init(Gtk::Box *container);
};
