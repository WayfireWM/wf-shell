#include "tray.hpp"
#include "gtkmm/enums.h"

void WayfireStatusNotifier::init(Gtk::Box *container)
{
	update_layout();
    icons_box.set_halign(Gtk::Align::FILL);
    icons_box.set_valign(Gtk::Align::FILL);
    icons_box.set_expand(true);
    icons_box.set_homogeneous(true);
    icons_box.get_style_context()->add_class("tray");
    icons_box.set_spacing(5);
    container->append(icons_box);
}

void WayfireStatusNotifier::add_item(const Glib::ustring & service)
{
    if (items.count(service) != 0)
    {
        return;
    }

    items.emplace(service, service);
    icons_box.append(items.at(service));
}

void WayfireStatusNotifier::remove_item(const Glib::ustring & service)
{
    if (items.count(service) == 0)
    {
        return;
    }

    icons_box.remove(items.at(service));
    items.erase(service);
}

void WayfireStatusNotifier::update_layout(){
    std::string panel_position = WfOption<std::string> {"panel/position"};

    if (panel_position == PANEL_POSITION_LEFT or panel_position == PANEL_POSITION_RIGHT){
        icons_box.set_orientation(Gtk::Orientation::VERTICAL);
    }

    else {
        icons_box.set_orientation(Gtk::Orientation::HORIZONTAL);
    }
}

void WayfireStatusNotifier::handle_config_reload(){
    update_layout();
}
