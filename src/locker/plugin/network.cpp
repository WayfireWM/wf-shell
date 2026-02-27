#include "network.hpp"
#include "gtkmm/enums.h"
#include "lockergrid.hpp"
#include "plugin.hpp"
#include "timedrevealer.hpp"
#include <memory>

WayfireLockerNetworkPluginWidget::WayfireLockerNetworkPluginWidget(std::string image_contents,
    std::string label_contents,
    std::string css_contents) :
    WayfireLockerTimedRevealer("locker/network_always")
{
    box.add_css_class("network");
    box.append(image);
    box.append(label);
    image.set_from_icon_name(image_contents);
    label.set_label(label_contents);
    label.add_css_class(css_contents);
    box.set_orientation(Gtk::Orientation::HORIZONTAL);
    set_child(box);
}

void WayfireLockerNetworkPluginWidget::set_connection(std::shared_ptr<Network> network)
{
    for (auto clas : box.get_css_classes())
    {
        if ((clas == "flat") || (clas == "network") || (clas == "widget-icon"))
        {
            continue;
        }

        box.remove_css_class(clas);
    }

    for (auto clas : network->get_css_classes())
    {
        box.add_css_class(clas);
    }

    label.set_label(network->get_name());
    image.set_from_icon_name(network->get_icon_symbolic());
}

WayfireLockerNetworkPlugin::WayfireLockerNetworkPlugin() :
    WayfireLockerPlugin("locker/network")
{}

void WayfireLockerNetworkPlugin::init()
{
    network_manager = NetworkManager::getInstance();
}

void WayfireLockerNetworkPlugin::deinit()
{}

void WayfireLockerNetworkPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerNetworkPluginWidget(image_contents, label_contents, css_contents));
    auto widget = widgets[id];
    /* Add to window */
    grid->attach(*widget, position);

    signals.push_back(network_manager->signal_default_changed().connect(
        sigc::mem_fun(*this, &WayfireLockerNetworkPlugin::set_connection)));
    set_connection(network_manager->get_primary_network());
}

void WayfireLockerNetworkPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.erase(id);
}

void WayfireLockerNetworkPlugin::set_connection(std::shared_ptr<Network> network)
{
    for (auto & it : widgets)
    {
        it.second->set_connection(network);
    }
}
