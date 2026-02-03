#pragma once
#include <memory>
#include <iostream>
#include <string>
#include <giomm.h>
#include <glibmm/variant.h>

#include "network.hpp"

#define CAP_5G 16
#define CAP_4G 8
#define CAP_3G 4
#define CAP_2G 2
#define CAP_CS 1

class ModemNetwork : public Network {
  public:
    unsigned char strength=0;
    int caps = 8;
    std::shared_ptr<Gio::DBus::Proxy> modem_proxy;
    std::shared_ptr<Gio::DBus::Proxy> mm_proxy;
    ModemNetwork(std::string path, std::shared_ptr<Gio::DBus::Proxy> device_proxy, std::shared_ptr<Gio::DBus::Proxy> modem_proxy):
        Network(path, device_proxy), modem_proxy(modem_proxy)
    {
        Glib::Variant<std::string> device_data;
        modem_proxy->get_cached_property(device_data, "DeviceId");

        find_mm_proxy(device_data.get());
    }

    void find_mm_proxy(std::string dev_id)
    {
        auto mm_om_proxy = Gio::DBus::Proxy::create_sync(modem_proxy->get_connection(),
            "org.freedesktop.ModemManager1",
            "/org/freedesktop/ModemManager1",
            "org.freedesktop.DBus.ObjectManager");
        
        auto ret1 = mm_om_proxy->call_sync("GetManagedObjects").get_child();
        auto ret = Glib::VariantBase::cast_dynamic<Glib::Variant<std::map<std::string, std::map<std::string, std::map<std::string, Glib::VariantBase>>>>>(ret1);

        for (auto &it : ret.get())
        {
            std::string modem_path = it.first;
            for (auto &next : it.second)
            {
                if (next.first == "org.freedesktop.ModemManager1.Modem")
                {
                    for (auto &why : next.second)
                    {
                        if (why.first == "DeviceIdentifier")
                        {
                            auto devid = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(why.second);
                            if (devid.get() == dev_id)
                            {

                                mm_proxy = Gio::DBus::Proxy::create_sync(device_proxy->get_connection(),
                                    "org.freedesktop.ModemManager1",
                                    modem_path,
                                    "org.freedeskop.ModemManager1.Modem");
                            }
                        }
                    }
                }
            }
        }
        if (mm_proxy)
        {
            mm_proxy->signal_properties_changed().connect(
                [this] (const Gio::DBus::Proxy::MapChangedProperties& properties, const std::vector<Glib::ustring>& invalidated)  {
                for (auto &it : properties)
                {
                    if (it.first == "SignalQuality")
                    {
                        auto container = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(it.second);
                        Glib::Variant<unsigned int> signal_data;
                        container.get_child(signal_data,0);
                        // "(ub)" => percent 0-100, 'is recent'
                        strength = signal_data.get();

                    }
                    if (it.first == "CurrentModes")
                    {
                        auto container = Glib::VariantBase::cast_dynamic<Glib::VariantContainerBase>(it.second);
                        Glib::Variant<unsigned int> mode_data;
                        container.get_child(mode_data,1);
                        caps = mode_data.get();
                    }
                }
            });
        } else
        {
            std::cerr << "Could not get extra modem details" << std::endl;
        }

    }

    ~ModemNetwork()
    {

    }

    std::string strength_string()
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

    std::string get_name()
    {
        if (!mm_proxy)
        {
            return "Misconfigured Mobile";
        }
        /* TODO Get Carrier from MM */
        return "Mobile";
    }

    std::string get_signal_band()
    {
        if (strength == 100)
        {
            return "100";
        } else if (strength >= 80)
        {
            return "80";
        } else if (strength >= 60)
        {
            return "60";
        } else if (strength >= 40)
        {
            return "40";
        } else if (strength >= 20)
        {
            return "20";
        }
        return "0";
    }

    std::string get_connection_type_string()
    {
        if (caps & CAP_5G)
        {
            return "5g";
        } else if (caps & CAP_4G)
        {
            return "lte";
        } else if (caps & CAP_3G)
        {
            return "hspa";
        } else if (caps & CAP_2G)
        {
            return "gprs";
        } else if (caps & CAP_CS)
        {
            return "edge";
        }
        return "edge";
    }

    std::string get_icon_name()
    {
        if (!mm_proxy)
        {
            return "network-mobile-off";
        }
        if (!is_active())
        {
            return "network-mobile-off";
        }
        return "network-mobile-"+get_signal_band()+"-"+get_connection_type_string();
    }

    std::string get_color_name()
    {
        return strength_string();
    }

    std::string get_friendly_name()
    {
        return "Mobile Data";
    }
};