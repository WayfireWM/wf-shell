#pragma once
#include <memory>
#include <sigc++/connection.h>
#include <giomm/dbusproxy.h>

#include "network.hpp"

/* Information about an active connection */
class Connection : public Network
{
  public:
    bool has_vpn;
    std::vector<sigc::connection> signals;
    std::shared_ptr<Gio::DBus::Proxy> connection_proxy, vpn_proxy;
    std::vector<std::shared_ptr<Network>> devices;
    std::string vpn_path = "";

    Connection();
    Connection(std::string path, std::shared_ptr<Gio::DBus::Proxy> connection_proxy,
        std::vector<std::shared_ptr<Network>> devices);
    ~Connection();
    std::string get_name();
    std::string get_icon_name();
    std::vector<std::string> get_css_classes();
};
