#pragma once

#include "multi-output-timed-plugin.hpp"
#include "widget-utils/clock.hpp"

class WayfireLockerClockPluginWidget : public WayfireLockerTimedWidget<ShellClock>
{
  public:
    WayfireLockerClockPluginWidget() :
        WayfireLockerTimedWidget("locker/clock_always", "clock")
    {}
};

class WayfireLockerClockPlugin :
    public WayfireLockerMultiOutputPlugin<WayfireLockerClockPluginWidget>
{
  protected:
    std::shared_ptr<WayfireLockerClockPluginWidget> create_widget() override
    {
        return std::make_shared<WayfireLockerClockPluginWidget>();
    }

  public:
    WayfireLockerClockPlugin();
};
