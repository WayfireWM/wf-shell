#include <memory>
#include <gtkmm/button.h>
#include <gtkmm/box.h>

#include "../locker.hpp"
#include "lockergrid.hpp"
#include "instant.hpp"

bool WayfireLockerInstantPlugin::should_enable()
{
    return (bool)enable;
}

void WayfireLockerInstantPlugin::add_output(int id, WayfireLockerGrid *grid)
{
    buttons.emplace(id, std::shared_ptr<Gtk::Button>(new Gtk::Button()));
    auto button = buttons[id];
    button->set_label("Press to unlock");
    button->add_css_class("instant-unlock");

    grid->attach(*button, WfOption<std::string>{"locker/instant_unlock_position"});

    button->signal_clicked().connect([] ()
    {
        WayfireLockerApp::get().unlock();
    }, false);
}

void WayfireLockerInstantPlugin::remove_output(int id)
{
    buttons.erase(id);
}

void WayfireLockerInstantPlugin::init()
{}
