#include "connection.hpp"
#include "network/manager.hpp"

Connection::Connection() :
    Network("/", nullptr), connection_proxy(nullptr), devices({})
{}

Connection::Connection(std::string path, std::shared_ptr<Gio::DBus::Proxy> connection_proxy,
    std::vector<std::shared_ptr<Network>> devices) :
    Network(path, nullptr), connection_proxy(connection_proxy), devices(devices)
{
    /* Bubble up emits from any device here */
    for (auto & it : devices)
    {
        signals.push_back(it->signal_network_altered().connect(
            [this] ()
        {
            network_altered.emit();
        }));
    }

    Glib::Variant<bool> vpn_data;
    connection_proxy->get_cached_property(vpn_data, "Vpn");
    has_vpn = vpn_data.get();

    if (has_vpn)
    {
        Glib::Variant<std::string> vpn_path_start;
        connection_proxy->get_cached_property(vpn_path_start, "Connection");
        vpn_path = vpn_path_start.get();
    }
}

Connection::~Connection()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

std::string Connection::get_name()
{
    if (devices.size() == 0)
    {
        return "No connection";
    }

    std::string secure = "";
    if (has_vpn)
    {
        auto settings = NetworkManager::getInstance()->get_vpn(vpn_path);
        if (settings)
        {
            secure = " with " + settings->name;
        } else
        {
            secure = " with VPN";
        }
    }

    return devices[0]->get_name() + secure;
}

std::string Connection::get_icon_name()
{
    if (devices.size() == 0)
    {
        return "network-disconnected";
    }

    std::string secure = "";
    if (has_vpn)
    {
        secure = devices[0]->get_secure_variant();
    }

    return devices[0]->get_icon_name() + secure;
}

std::vector<std::string> Connection::get_css_classes()
{
    if (devices.size() == 0)
    {
        return {"none"};
    }

    return devices[0]->get_css_classes();
}
