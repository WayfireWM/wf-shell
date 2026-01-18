#include <memory>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include "../locker.hpp"
#include "instant.hpp"

bool WayfireLockerInstantPlugin::should_enable()
{
    return (bool) enable;
}

void WayfireLockerInstantPlugin::add_output(int id, Gtk::Grid *grid)
{
    buttons.emplace(id, std::shared_ptr<Gtk::Button>(new Gtk::Button()));
    auto button = buttons[id];
    button->set_label("Press to unlock");
    button->add_css_class("instant-unlock");

    Gtk::Box* box = get_plugin_position(WfOption<std::string>{"locker/instant_unlock_position"}, grid);
    box->append(*button);

    button->signal_clicked().connect([](){
        WayfireLockerApp::get().unlock();
    }, false);
}

void WayfireLockerInstantPlugin::remove_output(int id)
{
    buttons.erase(id);
}

void WayfireLockerInstantPlugin::init()
{
    
}