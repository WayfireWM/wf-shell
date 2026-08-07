#pragma once

#include "plugin.hpp"
#include "lockergrid.hpp"
#include "timedrevealer.hpp"
#include "widget-utils/battery.hpp"

class WayfireLockerBatteryPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    ShellBattery battery;
    WayfireLockerBatteryPluginWidget();
};

class WayfireLockerBatteryPlugin : public WayfireLockerPlugin
{
  private:
    sigc::connection signal;

  public:
    WayfireLockerBatteryPlugin();
    void add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override
    {}
    void deinit() override
    {}
    void hide();
    void show();
    bool show_state = true;

    std::map<std::string, std::shared_ptr<WayfireLockerBatteryPluginWidget>> widgets;
};
