#include "wired.hpp"
WiredNetwork::WiredNetwork(std::string path, std::shared_ptr<Gio::DBus::Proxy> proxy) :
    Network(path, proxy)
{}

std::string WiredNetwork::get_name()
{
    return "Wired";
}

std::string WiredNetwork::get_icon_name()
{
    if (is_active())
    {
        return "network-wired";
    }

    return "network-wired-offline";
}

std::string WiredNetwork::get_color_name()
{
    return "excellent";
}

std::string WiredNetwork::get_friendly_name()
{
    return "Ethernet";
}
