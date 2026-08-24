#pragma once

#include <unordered_map>

#include "plugin.hpp"
#include "timedrevealer.hpp"
#include "lockergrid.hpp"
#include "widget-utils/clock.hpp"

class WayfireLockerClockPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    ShellClock label{"locker/clock_format"};
    WayfireLockerClockPluginWidget();
};

class WayfireLockerClockPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerClockPlugin();
    void add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override
    {}
    void deinit() override
    {}

    std::unordered_map<std::string, std::shared_ptr<WayfireLockerClockPluginWidget>> widgets;
};
