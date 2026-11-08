#pragma once

#include "multi-output-timed-plugin.hpp"
#include "widget-utils/weather.hpp"

class WayfireLockerWeatherPluginWidget : public WayfireLockerTimedWidget<ShellWeather>
{
  public:
    WayfireLockerWeatherPluginWidget() :
        WayfireLockerTimedWidget("locker/weather_always", "weather")
    {}
};

class WayfireLockerWeatherPlugin :
    public WayfireLockerMultiOutputPlugin<WayfireLockerWeatherPluginWidget>
{
  protected:
    std::shared_ptr<WayfireLockerWeatherPluginWidget> create_widget() override
    {
        return std::make_shared<WayfireLockerWeatherPluginWidget>();
    }

  public:
    WayfireLockerWeatherPlugin();
};
