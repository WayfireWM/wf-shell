#include "tray.hpp"

void WayfireStatusNotifier::init(Gtk::Box *container)
{
    icons_hbox.get_style_context()->add_class("tray");
    icons_hbox.set_spacing(5);
    container->append(icons_hbox);
}

void WayfireStatusNotifier::add_item(const Glib::ustring & service)
{
    if (items.count(service) != 0)
    {
        return;
    }

    items.emplace(service, service);
    icons_hbox.append(items.at(service));
}

void WayfireStatusNotifier::remove_item(const Glib::ustring & service)
{
    items.erase(service);
}
