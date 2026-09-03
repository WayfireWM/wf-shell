#pragma once

#include "multi-output-timed-plugin.hpp"
#include "widget-utils/battery.hpp"

class WayfireLockerBatteryPluginWidget : public WayfireLockerTimedWidget<ShellBattery>
{
  public:
    WayfireLockerBatteryPluginWidget() :
        WayfireLockerTimedWidget("locker/battery_always")
    {}
};

class WayfireLockerBatteryPlugin :
    public WayfireLockerMultiOutputPlugin<WayfireLockerBatteryPluginWidget>
{
  private:
    sigc::connection signal;

  protected:
    std::shared_ptr<WayfireLockerBatteryPluginWidget> create_widget() override
    {
        return std::make_shared<WayfireLockerBatteryPluginWidget>();
    }

  public:
    WayfireLockerBatteryPlugin();
};
