#include "network.hpp"
#include "manager.hpp"

type_signal_network_altered Network::signal_network_altered()
{
    return network_altered;
}

Network::Network(std::string path, std::shared_ptr<Gio::DBus::Proxy> in_proxy) :
    network_path(path), device_proxy(in_proxy)
{
    /* Allow for nullnetwork and pseudo networks */
    if (in_proxy == nullptr)
    {
        return;
    }

    Glib::Variant<Glib::ustring> val;
    device_proxy->get_cached_property(val, "Interface");
    if (val.get().length() > 0)
    {
        interface = val.get();
    }

    /* Any change of state */
    signals.push_back(device_proxy->signal_signal().connect(
        [this] (const Glib::ustring& sender, const Glib::ustring& signal,
                const Glib::VariantContainerBase& container)
    {
        if (signal == "StateChanged")
        {
            if (container.is_of_type(Glib::VariantType("(uuu)")))
            {
                auto value  = container.get_child(0);
                auto value2 = Glib::VariantBase::cast_dynamic<Glib::Variant<unsigned int>>(value).get();
                last_state  = value2;
                network_altered.emit();
            }
        }
    }));

    signals.push_back(device_proxy->signal_properties_changed().connect(
        [this] (const Gio::DBus::Proxy::MapChangedProperties& properties,
                const std::vector<Glib::ustring>& invalidated)
    {
        for (auto & it : properties)
        {
            if (it.first == "ActiveConnection")
            {
                network_altered.emit();
            }
        }
    }));
}

Network::~Network()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

std::string Network::get_friendly_name()
{
    return "Unknown Device";
}

std::string Network::get_interface()
{
    if (!device_proxy)
    {
        return "/dev/null";
    }

    Glib::Variant<std::string> iface;
    device_proxy->get_cached_property(iface, "Interface");
    return iface.get();
}

bool Network::show_spinner()
{
    return last_state == NM_DEVICE_STATE_PREPARE ||
           last_state == NM_DEVICE_STATE_CONFIG;
}

std::string Network::get_path()
{
    return network_path;
}

void Network::connect(std::string path)
{
    NetworkManager::getInstance()->activate_connection("/", network_path, path);
}

void Network::toggle()
{
    if (is_active())
    {
        disconnect();
    } else
    {
        connect("/");
    }
}

void Network::disconnect()
{
    device_proxy->call_sync("Disconnect");
}

bool Network::is_active()
{
    Glib::Variant<std::string> val;
    device_proxy->get_cached_property(val, "ActiveConnection");
    return val.get() != "/";
}
