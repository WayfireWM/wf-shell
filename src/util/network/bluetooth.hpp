#pragma once

#include <memory>
#include <giomm.h>

#include "network.hpp"
class BluetoothNetwork : public Network
{
  public:
    std::shared_ptr<Gio::DBus::Proxy> bluetooth_proxy;
    BluetoothNetwork(std::string path, std::shared_ptr<Gio::DBus::Proxy> device_proxy,
        std::shared_ptr<Gio::DBus::Proxy> bluetooth_proxy) :
        Network(path, device_proxy), bluetooth_proxy(bluetooth_proxy)
    {}

    std::string get_name() override
    {
        return "";
    }

    std::string get_icon_name() override
    {
        if (is_active())
        {
            return "network-bluetooth-activated";
        }

        return "network-bluetooth-inactive";
    }

    std::vector<std::string> get_css_classes() override
    {
        return {};
    }

    std::string get_friendly_name() override
    {
        return "Bluetooth";
    }
};
