#include "network.hpp"
#include "network/network.hpp"
#include <glibmm/spawn.h>
#include <cassert>
#include <gtk-utils.hpp>
#include <memory>

WayfireNetworkInfo::WayfireNetworkInfo()
{}

void WayfireNetworkInfo::init(Gtk::Box *container)
{
    network_manager = NetworkManager::getInstance();
    button = std::make_unique<WayfireMenuButton>("panel");
    button->add_css_class("widget-icon");
    button->add_css_class("flat");
    button->add_css_class("network");

    container->append(*button);
    button->set_child(button_content);
    button->add_css_class("flat");

    button->get_popover()->set_child(control);

    button_content.set_valign(Gtk::Align::CENTER);
    button_content.append(icon);
    button_content.append(status);
    button_content.set_spacing(6);

    icon.set_valign(Gtk::Align::CENTER);

    no_label.set_callback([this] ()
    {
        if (no_label)
        {
            status.hide();
        } else
        {
            status.show();
        }
    });

    signals.push_back(network_manager->signal_default_changed().connect(
        sigc::mem_fun(*this, &WayfireNetworkInfo::set_connection)));
    set_connection(network_manager->get_primary_network());
}

void WayfireNetworkInfo::set_connection(std::shared_ptr<Network> network)
{
    for (auto clas : button_content.get_css_classes())
    {
        if ((clas == "flat") || (clas == "network") || (clas == "widget-icon"))
        {
            continue;
        }

        button_content.remove_css_class(clas);
    }

    for (auto clas : network->get_css_classes())
    {
        button_content.add_css_class(clas);
    }

    status.set_label(network->get_name());
    icon.set_from_icon_name(network->get_icon_symbolic());
}

WayfireNetworkInfo::~WayfireNetworkInfo()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}
