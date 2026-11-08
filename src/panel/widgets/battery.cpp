#include <gtk-utils.hpp>

#include "battery.hpp"

void WayfireBatteryInfo::update_layout()
{
    WfOption<std::string> panel_position{"panel/position"};

    if (panel_position.value() == PANEL_POSITION_LEFT or panel_position.value() == PANEL_POSITION_RIGHT)
    {
        battery.set_orientation(Gtk::Orientation::VERTICAL);
    } else
    {
        battery.set_orientation(Gtk::Orientation::HORIZONTAL);
    }
}

void WayfireBatteryInfo::handle_config_reload()
{
    update_layout();
}

void WayfireBatteryInfo::init(Gtk::Box *container)
{
    panel_menu = std::make_unique<WayfireMenuWidget>("panel", "battery");

    panel_menu->set_child(battery);

    update_layout();

    if (do_power_modes)
    {
        panel_menu->open_on(1);
        panel_menu->insert_action_group("actions", power_tools.actions);
        panel_menu->set_spacing(5);
        panel_menu->set_menu_model(power_tools.profiles_menu);
    }

    container->append(*panel_menu);
}
