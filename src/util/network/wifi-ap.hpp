#pragma once
#include "giomm.h"
#include "giomm/dbusproxy.h"
#include "glibmm/variant.h"
#include "network.hpp"
#include "sigc++/connection.h"
#include <memory>
#include <iostream>

using type_signal_access_point_altered = sigc::signal<void (void)>;
class AccessPoint {
  private:
    std::string ap_path;
    type_signal_access_point_altered access_point_altered;
    unsigned char strength;
    std::string ssid="";
  public:
    std::string get_path();
    std::shared_ptr<Gio::DBus::Proxy> access_point_proxy;
    AccessPoint(std::string path, std::shared_ptr<Gio::DBus::Proxy> access_point_proxy);
    std::string get_ssid();
    std::string strength_string();
    std::string get_icon_name();
    type_signal_access_point_altered signal_altered();
};
