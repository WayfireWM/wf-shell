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
    container->append(battery);
    update_layout();
}
