#pragma once
#include <unordered_map>

#include "plugin.hpp"
#include "timedrevealer.hpp"
#include "lockergrid.hpp"

#include "widget-utils/weather.hpp"

class WayfireLockerWeatherPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    ShellWeather weather;
    WayfireLockerWeatherPluginWidget();
};

class WayfireLockerWeatherPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerWeatherPlugin();
    void add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override
    {}
    void deinit() override
    {}

    void hide();
    void show();

    std::unordered_map<std::string, std::shared_ptr<WayfireLockerWeatherPluginWidget>> weather_widgets;
    bool shown = true;
};
