#include <memory>
#include <glibmm.h>
#include <gtkmm/box.h>

#include "lockergrid.hpp"
#include "timedrevealer.hpp"
#include "clock.hpp"


WayfireLockerClockPluginWidget::WayfireLockerClockPluginWidget() :
    WayfireLockerTimedRevealer("locker/clock_always")
{
    set_child(label);
    label.add_css_class("clock");
}

void WayfireLockerClockPlugin::add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerClockPluginWidget());
    auto widget = widgets[id];

    grid->attach(*widget, position);
}

void WayfireLockerClockPlugin::remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*widgets[id]);
    widgets.erase(id);
}

WayfireLockerClockPlugin::WayfireLockerClockPlugin() :
    WayfireLockerPlugin("locker/clock")
{}
