#include <memory>
#include <glibmm.h>

#include "lockergrid.hpp"
#include "weather.hpp"
#include "timedrevealer.hpp"

WayfireLockerWeatherPluginWidget::WayfireLockerWeatherPluginWidget() :
    WayfireLockerTimedRevealer("locker/weather_always")
{
    set_child(weather);
    weather.add_css_class("weather");
}

void WayfireLockerWeatherPlugin::add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid)
{
    weather_widgets.emplace(id, new WayfireLockerWeatherPluginWidget());

    auto weather_widget = weather_widgets[id];
    if (!shown)
    {
        weather_widget->hide();
    }

    grid->attach(*weather_widget, position);
}

void WayfireLockerWeatherPlugin::remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*weather_widgets[id]);
    weather_widgets.erase(id);
}

WayfireLockerWeatherPlugin::WayfireLockerWeatherPlugin() :
    WayfireLockerPlugin("locker/weather")
{}

void WayfireLockerWeatherPlugin::hide()
{
    for (auto& it : weather_widgets)
    {
        it.second->hide();
    }

    shown = false;
}

void WayfireLockerWeatherPlugin::show()
{
    for (auto& it : weather_widgets)
    {
        it.second->show();
    }

    shown = true;
}
