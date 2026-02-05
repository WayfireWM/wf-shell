#pragma once
#include <string>
#include "network.hpp"

class WiredNetwork : public Network
{
  public:
    WiredNetwork(std::string path, std::shared_ptr<Gio::DBus::Proxy> proxy);
    std::string get_name() override;
    std::string get_icon_name() override;
    std::string get_friendly_name() override;
    std::vector<std::string> get_css_classes() override;
    std::string get_secure_variant() override;
};
