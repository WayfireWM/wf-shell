#pragma once
#include <giomm/dbusproxy.h>
#include <glibmm/variant.h>
using type_signal_vpn_state = sigc::signal<void (bool)>;
class VpnConfig
{
  private:
    std::shared_ptr<Gio::DBus::Proxy> vpn_proxy;
    type_signal_vpn_state state_changed;

    bool is_active = false;

  public:
    std::string name;
    std::string path;
    std::string connection_path;
    type_signal_vpn_state signal_state_changed()
    {
        return state_changed;
    }

    bool get_active()
    {
        return is_active;
    }

    void set_active(bool active)
    {
        if (is_active == active)
        {
            return;
        }

        is_active = active;
        state_changed.emit(is_active);
    }

    void set_connection_path(std::string path)
    {
        connection_path = path;
    }

    std::string get_connection_path()
    {
        return connection_path;
    }

    VpnConfig(std::string path, std::shared_ptr<Gio::DBus::Proxy> vpn_proxy, std::string name) :
        vpn_proxy(vpn_proxy), name(name), path(path)
    {}
};
