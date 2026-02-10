#include "tray.hpp"

void WayfireStatusNotifier::init(Gtk::Box *container)
{
    icons_box.add_css_class("tray");
    update_layout();
    icons_box.set_halign(Gtk::Align::FILL);
    icons_box.set_valign(Gtk::Align::FILL);
    icons_box.set_expand(true);
    icons_box.set_homogeneous(true);
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

void WayfireStatusNotifier::update_layout()
{
    icons_box.set_spacing(spacing);
}

void WayfireStatusNotifier::handle_config_reload()
{
    update_layout();
}
