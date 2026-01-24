#include <memory>
#include <gtkmm/button.h>
#include <gtkmm/box.h>

#include "locker.hpp"
#include "lockergrid.hpp"
#include "plugin.hpp"
#include "instant.hpp"

WayfireLockerInstantPlugin::WayfireLockerInstantPlugin():
    WayfireLockerPlugin("locker/instant_unlock_enable", "locker/instant_unlock_position")
{ }

void WayfireLockerInstantPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    buttons.emplace(id, std::shared_ptr<Gtk::Button>(new Gtk::Button()));
    auto button = buttons[id];
    button->set_label("Press to unlock");
    button->add_css_class("instant-unlock");

    grid->attach(*button, position);

    button->signal_clicked().connect([] ()
    {
        WayfireLockerApp::get().perform_unlock();
    }, false);
}

void WayfireLockerInstantPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*buttons[id]);
    buttons.erase(id);
}

void WayfireLockerInstantPlugin::init()
{}

void WayfireLockerInstantPlugin::deinit()
{}