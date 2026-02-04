#include <glibmm.h>
#include <memory>

#include "wifi-ap.hpp"
#include "wifi.hpp"

type_signal_access_point WifiNetwork::signal_add_access_point()
{
     return add_ap;
}
type_signal_access_point WifiNetwork::signal_remove_access_point()
{ 
    return remove_ap;
}

std::map<std::string, std::shared_ptr<AccessPoint>> WifiNetwork::get_access_points()
{
    return all_access_points;
}

void WifiNetwork::add_access_point(std::string path)
{

    auto ap_proxy =Gio::DBus::Proxy::create_sync(wifi_proxy->get_connection(),
        "org.freedesktop.NetworkManager",
        path,
        "org.freedesktop.NetworkManager.AccessPoint");
    all_access_points.emplace(path, std::make_shared<AccessPoint>(path, ap_proxy));
}

void WifiNetwork::remove_access_point(std::string path)
{
    all_access_points.erase(path);
}

WifiNetwork::WifiNetwork(std::string path, std::shared_ptr<Gio::DBus::Proxy> device_proxy, std::shared_ptr<Gio::DBus::Proxy> wifi_proxy):
    Network(path, device_proxy), wifi_proxy(wifi_proxy)
{
    signals.push_back(wifi_proxy->signal_signal().connect(
        [this] (const Glib::ustring& source, const Glib::ustring& signal , const Glib::VariantContainerBase& value) {
            if (signal == "AccessPointAdded")
            {
                auto val = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(value.get_child()).get();
                add_access_point(val);
                add_ap.emit(all_access_points[val]);
            } else if (signal == "AccessPointRemoved")
            {
                auto val = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(value.get_child()).get();
                remove_ap.emit(all_access_points[val]);
                all_access_points.erase(val);
            }
        }
    ));

    signals.push_back(wifi_proxy->signal_properties_changed().connect(
        [this] (const Gio::DBus::Proxy::MapChangedProperties& properties, const std::vector<Glib::ustring>& invalidated) {
            for (auto &it : properties)
            {
                if (it.first == "ActiveAccessPoint")
                {
                    if (access_point_signal)
                    {
                        access_point_signal.disconnect();
                    }
                    auto access_point = get_current_access_point();
                    if (access_point)
                    {
                        /* Bubble signal upwards */
                        access_point_signal = access_point->signal_altered().connect(
                            [this] () {
                                network_altered.emit();
                            }
                        );
                    }
                    network_altered.emit();
                    
                }
            }
        }
    ));
    /* Initial values */
    Glib::Variant<std::vector<std::string>> base;
    wifi_proxy->get_cached_property(base, "AccessPoints");
    for (auto &it : base.get())
    {
        add_access_point(it);
    }

    auto access_point = get_current_access_point();
    if (access_point)
    {
        signals.push_back(access_point_signal = access_point->signal_altered().connect(
            [this]() {
                network_altered.emit();
            }
        ));
    }
}

std::string WifiNetwork::get_name()
{
    auto network = get_current_access_point();
    if (network)
    {
        return network->get_ssid();
    }
    return "Wifi...";
}

std::string WifiNetwork::get_color_name()
{
    return "";
}

std::string WifiNetwork::get_icon_name()
{
    if (show_spinner())
    {
        return "network-wireless-disconnected";
    }
    auto ap = get_current_access_point();
    if (!ap){
        return "network-wireless-disconnected";
    }
    return "network-wireless-signal-"+ap->strength_string();
}

std::string WifiNetwork::get_friendly_name()
{
    auto ap = get_current_access_point();
    if (ap)
    {
        return ap->get_ssid();
    }
    return "Not connected";
}

std::shared_ptr<AccessPoint> WifiNetwork::get_current_access_point()
{
    auto access_point = all_access_points[get_current_access_point_path()];
    return access_point;
}

std::string WifiNetwork::get_current_access_point_path()
{
    Glib::Variant<std::string> ap_name;
    wifi_proxy->get_cached_property(ap_name, "ActiveAccessPoint");
    return ap_name.get();
}

WifiNetwork::~WifiNetwork()
{
    all_access_points.clear();
    if(access_point_signal)
    {
        access_point_signal.disconnect();
    }
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}