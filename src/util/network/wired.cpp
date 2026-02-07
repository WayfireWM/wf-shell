#include "wired.hpp"
#include <vector>
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

std::vector<std::string> WiredNetwork::get_css_classes()
{
    return {"ethernet", "good"};
}

std::string WiredNetwork::get_friendly_name()
{
    return "Ethernet";
}

std::string WiredNetwork::get_secure_variant()
{
    return "-secure";
}
