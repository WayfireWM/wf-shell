#include <memory>
#include <gtkmm/button.h>
#include <gtkmm/box.h>

#include "locker.hpp"
#include "lockergrid.hpp"
#include "plugin.hpp"
#include "timedrevealer.hpp"
#include "instant.hpp"

WayfireLockerInstantPlugin::WayfireLockerInstantPlugin():
    WayfireLockerPlugin("locker/instant_unlock")
{ }

WayfireLockerInstantPluginWidget::WayfireLockerInstantPluginWidget():
    WayfireLockerTimedRevealer("locker/instant_unlock_always")
{
    set_child(button);
    button.set_label("Unlock now");
    button.add_css_class("instant-unlock");
}

void WayfireLockerInstantPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerInstantPluginWidget());
    auto widget = widgets[id];
    grid->attach(*widget, position);

    widget->button.signal_clicked().connect([] ()
    {
        WayfireLockerApp::get().perform_unlock("Instant unlock pressed");
    }, false);
}

void WayfireLockerInstantPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*widgets[id]);
    widgets.erase(id);
}

void WayfireLockerInstantPlugin::init()
{}

void WayfireLockerInstantPlugin::deinit()
{}