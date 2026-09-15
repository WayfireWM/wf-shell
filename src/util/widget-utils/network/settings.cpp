#include "settings.hpp"
#include "glibmm/variant.h"
#include "sigc++/functors/mem_fun.h"
#include "giomm/dbusproxy.h"
#include <memory>

NetworkSettings::NetworkSettings(std::string path, std::shared_ptr<Gio::DBus::Proxy> proxy) :
    proxy(proxy)
{
    proxy->signal_signal().connect(
        sigc::mem_fun(*this, &NetworkSettings::signal));
    read_contents();
}

void NetworkSettings::signal(const Glib::ustring& name, const Glib::ustring& signal,
    const Glib::VariantContainerBase& variants)
{
    if (signal == "Updates")
    {
        read_contents();
    }
}

void NetworkSettings::read_contents()
{
    auto contents = Glib::VariantBase::cast_dynamic<Glib::Variant<std::map<std::string,
        std::map<std::string, Glib::VariantBase>>>>(proxy->call_sync("GetSettings").get_child()).get();

    setting_name =
        Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(contents["connection"]["id"]).get();

    if (contents.count("802-11-wireless") == 1)
    {
        auto ssid_bytes =
            Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<unsigned char>>>(contents[
                "802-11-wireless"][
                    "ssid"]).get();
        ssid = std::string(ssid_bytes.begin(), ssid_bytes.end());
    }
}

std::string NetworkSettings::get_ssid()
{
    return ssid;
}

std::string NetworkSettings::get_name()
{
    return setting_name;
}
