#pragma once

#include <sigc++/connection.h>

#include "widget.hpp"
#include "widget-utils/battery.hpp"
#include "widget-utils/power-profiles.hpp"

class WayfireBatteryInfo : public WayfireWidget
{
    ShellBattery battery;
    ShellPowerTools power_tools;
    std::unique_ptr<WayfireMenuWidget> panel_menu;

    WfOption<bool> do_power_modes{"panel/battery_power_modes"};

    void update_layout();
    void handle_config_reload();

  public:
    WayfireBatteryInfo() : battery("panel")
    {}
    virtual void init(Gtk::Box *container);
};
