#pragma once
#include "giomm/dbusproxy.h"
#include "glibmm/variant.h"
#include "network.hpp"
#include <memory>

class VpnConfig {
  private:
    std::shared_ptr<Gio::DBus::Proxy> vpn_proxy;
  public:
    std::string name;
    std::string path;

    VpnConfig(std::string path, std::shared_ptr<Gio::DBus::Proxy> vpn_proxy, std::string name):
        vpn_proxy(vpn_proxy), name(name),path(path)
    {

    }


};