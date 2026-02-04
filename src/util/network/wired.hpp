#pragma once
#include <string>
#include "network.hpp"

class WiredNetwork : public Network
{
  public:
    WiredNetwork(std::string path, std::shared_ptr<Gio::DBus::Proxy> proxy);
    std::string get_name() override;
    std::string get_icon_name() override;
    std::string get_color_name() override;
    std::string get_friendly_name() override;
};
