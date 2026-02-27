#pragma once
#include <memory>
#include <iostream>
#include <string>
#include <giomm.h>
#include <glibmm/variant.h>

#include "network.hpp"
#include "sigc++/connection.h"

#define CAP_5G 16
#define CAP_4G 8
#define CAP_3G 4
#define CAP_2G 2
#define CAP_CS 1

class ModemNetwork : public Network
{
  public:
    unsigned char strength = 0;
    int caps = 8;
    std::shared_ptr<Gio::DBus::Proxy> modem_3gpp_proxy;
    std::shared_ptr<Gio::DBus::Proxy> modem_proxy;
    std::shared_ptr<Gio::DBus::Proxy> mm_proxy;
    ModemNetwork(std::string path, std::shared_ptr<Gio::DBus::Proxy> device_proxy,
        std::shared_ptr<Gio::DBus::Proxy> modem_proxy);
    void find_mm_proxy(std::string dev_id);
    ~ModemNetwork();
    std::string strength_string();
    std::string get_name() override;
    std::string get_signal_band();
    std::string get_connection_type_string();
    std::string get_icon_name() override;
    std::string get_friendly_name() override;
    std::string get_secure_variant() override;
    std::vector<std::string> get_css_classes() override;
};
