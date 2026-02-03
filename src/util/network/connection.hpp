#pragma once
#include "giomm/dbusproxy.h"
#include "network.hpp"
#include "sigc++/connection.h"
#include <memory>
/* Information about an active connection */
class Connection : public Network {
  public:
    bool has_vpn;
    std::vector<sigc::connection> signals;
    std::shared_ptr<Gio::DBus::Proxy> connection_proxy, vpn_proxy;
    std::vector<std::shared_ptr<Network>> devices;
    Connection(std::string path, std::shared_ptr<Gio::DBus::Proxy> connection_proxy, std::vector<std::shared_ptr<Network>> devices):
        Network(path, nullptr), connection_proxy(connection_proxy), devices(devices)
    {
        /* Bubble up emits from any device here */
        for(auto &it : devices)
        {
            signals.push_back(it->signal_network_altered().connect(
                [this] () {
                    network_altered.emit();
                }
            ));
        }

        Glib::Variant<bool> vpn_data;
        connection_proxy->get_cached_property(vpn_data, "Vpn");
        has_vpn = vpn_data.get();
    }

    ~Connection()
    {
        for (auto signal : signals){
            signal.disconnect();
        }
    }

    std::string get_name()
    {
        if (devices.size() ==0)
        {
            return "No connection";
        }
        std::string secure = "";
        if (has_vpn)
        {
            secure = " with VPN";
        }
        return devices[0]->get_name()+secure;
    }

    std::string get_icon_name(){
        if (devices.size() == 0)
        {
            return "network-disconnected";
        }
        std::string secure = "";
        if (has_vpn)
        {
            secure = "-secure";
        }
        return devices[0]->get_icon_name()+secure;
    }

    std::string get_color_name(){
        if (devices.size() == 0)
        {
            return "none";
        }
        return devices[0]->get_color_name();
    }

};