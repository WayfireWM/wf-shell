#include <memory>

#include "lockergrid.hpp"
#include "timedrevealer.hpp"
#include "battery.hpp"

WayfireLockerBatteryPluginWidget::WayfireLockerBatteryPluginWidget() :
    WayfireLockerTimedRevealer("locker/battery_always"),
    battery("locker")
{
    set_child(battery);
}

void WayfireLockerBatteryPlugin::add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerBatteryPluginWidget());
    auto widget = widgets[id];

    if (!show_state)
    {
        widget->hide();
    }

    grid->attach(*widget, (std::string)position);
}

void WayfireLockerBatteryPlugin::remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*widgets[id]);
    widgets.erase(id);
}

void WayfireLockerBatteryPlugin::hide()
{
    show_state = false;
    for (auto& it : widgets)
    {
        it.second->hide();
    }
}

void WayfireLockerBatteryPlugin::show()
{
    show_state = true;
    for (auto& it : widgets)
    {
        it.second->show();
    }
}

WayfireLockerBatteryPlugin::WayfireLockerBatteryPlugin() :
    WayfireLockerPlugin("locker/battery")
{}
