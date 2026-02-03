#pragma once
#include <memory>
#include <sigc++/signal.h>
#include <sigc++/connection.h>
#include <vector>
#include <map>
#include <giomm.h>
#include <glibmm.h>

#include "network.hpp"
#include "connection.hpp"
#include "network/null.hpp"
#include "vpn.hpp"

using type_signal_network = sigc::signal<void (std::shared_ptr<Network>)>;
using type_signal_device_list_changed = sigc::signal<void ( std::map<std::string, std::shared_ptr<Network>>)>;
using type_signal_simple = sigc::signal<void (void)>;
using type_signal_path = sigc::signal<void (std::string)>;

class NetworkManager 
{
  private:
    type_signal_network default_changed, device_added, device_removed;
    type_signal_device_list_changed device_list_changed;
    type_signal_simple global_change, nm_start, nm_stop;
    type_signal_path vpn_added, vpn_removed;
    

    Glib::RefPtr<Gio::DBus::Connection> connection;
    Glib::RefPtr<Gio::DBus::Proxy> nm_proxy, settings_proxy, manager_proxy;

    std::vector<sigc::connection> nm_signals, dbus_signals;
    sigc::connection debounce, primary_signal;

    std::string primary_connection = "";

    std::shared_ptr<Connection> primary_connection_obj = std::make_shared<Connection>();

    std::map<std::string, std::shared_ptr<Network>> all_devices;
    std::map<std::string, std::shared_ptr<VpnConfig>> all_vpns;

    void on_nm_properties_changed(const Gio::DBus::Proxy::MapChangedProperties& properties, const std::vector<Glib::ustring>& invalidated);
    void on_nm_signal(const Glib::ustring&, const Glib::ustring&, const Glib::VariantContainerBase&);
    void get_all_devices_cb(std::shared_ptr<Gio::AsyncResult>);
    void add_network(std::string path);
    void check_add_vpn(std::string path);
    void changed_primary(std::string path);
    void connect_nm();
    void lost_nm();

  public:
    /* Emitted when the default connection or it's properties change */
    type_signal_network signal_default_changed() { return default_changed; }
    /* Emitted when any network device is connected or disconnected */
    type_signal_device_list_changed signal_device_list_changed() { return device_list_changed; }
    /* Emitted when a networking device is added */
    type_signal_network signal_device_added() { return device_added; }
    /* Emitted when a networking device is removed */
    type_signal_network signal_device_removed() { return device_removed; }
    /* Emitted when any of the global enable toggles is changed */
    type_signal_simple signal_global_toggle() { return global_change; }
    type_signal_path signal_vpn_added() { return vpn_added; }
    type_signal_path signal_vpn_removed() { return vpn_removed; }
    type_signal_simple signal_nm_start() { return nm_start; }
    type_signal_simple signal_nm_stop() { return nm_stop; }
    std::shared_ptr<Gio::DBus::Proxy> get_nm_proxy() { return nm_proxy; }
    /* A list of current networks. */

    std::map<std::string, std::shared_ptr<VpnConfig>> get_all_vpns() {return all_vpns; }
    std::map<std::string, std::shared_ptr<Network>> get_all_devices() { return all_devices; }
    
    /* TODO Consider allowing this to lose last reference */
    static std::shared_ptr<NetworkManager> getInstance()
    {
        static std::shared_ptr<NetworkManager> instance;
        if (!instance)
        {
          instance = std::make_shared<NetworkManager>();
        }
        return instance;
    }
    NetworkManager();
    ~NetworkManager();
    
    std::shared_ptr<Connection> get_primary_network();
    std::shared_ptr<Connection> get_connection(std::string path);
    void activate_connection(std::string connection_path, std::string device_path, std::string details_path);
    void deactivate_connection(std::string connection_path);


    std::tuple<bool, bool> wifi_global_enabled();
    std::tuple<bool, bool> mobile_global_enabled();
    bool networking_global_enabled();

    void wifi_global_set(bool value);
    void mobile_global_set(bool value);
    void networking_global_set(bool value);
};