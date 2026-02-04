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

    signals.push_back(network_manager->signal_default_changed().connect(
        sigc::mem_fun(*this, &WayfireNetworkInfo::set_connection)));
    set_connection(network_manager->get_primary_network());
}

void WayfireNetworkInfo::set_connection(std::shared_ptr<Network> network)
{
    status.remove_css_class("none");
    status.remove_css_class("weak");
    status.remove_css_class("bad");
    status.remove_css_class("ok");
    status.remove_css_class("excellent");

    status.set_label(network->get_name());
    icon.set_from_icon_name(network->get_icon_symbolic());
    auto color = network->get_color_name();
    if (color != "")
    {
        status.add_css_class(color);
    }
}

WayfireNetworkInfo::~WayfireNetworkInfo()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}
