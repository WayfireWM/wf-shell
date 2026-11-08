#include "wifi-ap.hpp"
#include "glibmm/variant.h"
#include "network.hpp"
#include <memory>

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

    Glib::Variant<unsigned int> security_flags_start;
    access_point_proxy->get_cached_property(security_flags_start, "RsnFlags");
    security_flags = security_flags_start.get();

    Glib::Variant<unsigned int> freq_start;
    access_point_proxy->get_cached_property(freq_start, "Frequency");
    freq = freq_start.get();

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
            } else if (it.first == "RsnFlags")
            {
                auto value =
                    Glib::VariantBase::cast_dynamic<Glib::Variant<unsigned int>>(it.second).get();
                security_flags = value;
                access_point_altered.emit();
            } else if (it.first == "Frequency")
            {
                auto value =
                    Glib::VariantBase::cast_dynamic<Glib::Variant<unsigned int>>(it.second).get();
                freq = value;
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

std::string AccessPoint::get_security_icon_name()
{
    if (security_flags | NM_SOME_WPA_SECURITY)
    {
        return "channel-secure-symbolic";
    }

    return "channel-insecure-symbolic";
}

std::string AccessPoint::get_band_name()
{
    if ((freq > 800) && (freq < 1000))
    {
        return "900Mhz";
    } else if ((freq > 2000) && (freq < 3000))
    {
        return "2.4 Ghz";
    } else if ((freq >= 5000) && (freq < 6000))
    {
        return "5 Ghz";
    } else if ((freq >= 6000) && (freq < 7000))
    {
        return "6 Ghz";
    } else if ((freq >= 40000) && (freq < 50000))
    {
        return "45 Ghz";
    } else if ((freq >= 57000) && (freq < 74000))
    {
        return "60 Ghz";
    }

    return "???";
}

std::vector<std::string> AccessPoint::get_css_classes()
{
    /* Set a bunch of AP-specific info here
     *  This allows theme makers to put much more detail in */
    std::vector<std::string> classlist;
    classlist.push_back("access-point");
    if ((freq > 800) && (freq < 1000))
    {
        classlist.push_back("f900mhz");
    } else if ((freq > 2000) && (freq < 3000))
    {
        classlist.push_back("f2-4ghz");
    } else if ((freq >= 5000) && (freq < 6000))
    {
        classlist.push_back("f5ghz");
    } else if ((freq >= 6000) && (freq < 7000))
    {
        classlist.push_back("f6ghz");
    } else if ((freq >= 40000) && (freq < 50000))
    {
        classlist.push_back("f45ghz");
    } else if ((freq >= 57000) && (freq < 74000))
    {
        classlist.push_back("f60ghz");
    }

    classlist.push_back(strength_string());

    if (ssid.length() > 0)
    {
        classlist.push_back("ap-" + ssid);
    }

    if (security_flags | NM_SOME_WPA_SECURITY)
    {
        classlist.push_back("secure");
    } else
    {
        classlist.push_back("insecure");
    }

    if (saved_password)
    {
        classlist.push_back("has-password");
    }

    return classlist;
}

unsigned char AccessPoint::get_strength()
{
    return strength;
}

void AccessPoint::set_has_saved_password(bool new_val)
{
    saved_password = new_val;
    access_point_altered.emit();
}

bool AccessPoint::has_saved_password()
{
    return saved_password;
}
