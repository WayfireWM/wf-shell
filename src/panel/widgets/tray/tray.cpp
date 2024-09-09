#include "tray.hpp"

void WayfireStatusNotifier::init(Gtk::HBox *container)
{
    icons_hbox.get_style_context()->add_class("tray");
    container->add(icons_hbox);
}

void WayfireStatusNotifier::add_item(const Glib::ustring & service)
{
    if (items.count(service) != 0)
    {
        return;
    }

    items.emplace(service, service);
    icons_hbox.pack_start(items.at(service), Gtk::PACK_SHRINK);
    icons_hbox.show_all();
}

void WayfireStatusNotifier::remove_item(const Glib::ustring & service)
{
    items.erase(service);
}
