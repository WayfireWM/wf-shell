#include "wifi-ap.hpp"
#include "network.hpp"

std::string AccessPoint::get_path()
{
    return ap_path;
}

AccessPoint::AccessPoint(std::string path, std::shared_ptr<Gio::DBus::Proxy> access_point_proxy) :
    ap_path(path), access_point_proxy(access_point_proxy)
{
    Glib::Variant<unsigned char> strength_start;
    access_point_proxy->get_cached_property(strength_start, "Strength");
    strength = strength_start.get();

    Glib::Variant<std::vector<unsigned char>> ssid_start;
    access_point_proxy->get_cached_property(ssid_start, "Ssid");
    auto ssid_bytes = ssid_start.get();
    ssid = std::string(ssid_bytes.begin(), ssid_bytes.end());

    signals.push_back(access_point_proxy->signal_properties_changed().connect(
        [this] (const Gio::DBus::Proxy::MapChangedProperties& properties,
                const std::vector<Glib::ustring>& invalidated)
    {
        for (auto & it : properties)
        {
            if (it.first == "Strength")
            {
                auto value = Glib::VariantBase::cast_dynamic<Glib::Variant<unsigned char>>(it.second).get();
                strength   = value;
                access_point_altered.emit();
            } else if (it.first == "Ssid")
            {
                auto value =
                    Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<unsigned char>>>(it.second).get();
                ssid = std::string(value.begin(), value.end());
                access_point_altered.emit();
            }
        }
    }));
}

std::string AccessPoint::get_ssid()
{
    return ssid;
}

std::string AccessPoint::strength_string()
{
    if (strength >= 80)
    {
        return "excellent";
    }

    if (strength >= 55)
    {
        return "good";
    }

    if (strength >= 30)
    {
        return "ok";
    }

    if (strength >= 5)
    {
        return "weak";
    }

    return "none";
}

std::string AccessPoint::get_icon_name()
{
    return "network-wireless-signal-" + strength_string() + "-symbolic";
}

type_signal_network_altered AccessPoint::signal_altered()
{
    return access_point_altered;
}

AccessPoint::~AccessPoint()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}
