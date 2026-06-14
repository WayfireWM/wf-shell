#include <cassert>
#include <giomm.h>
#include <glibmm.h>
#include <iostream>
#include <memory>
#include <string>

#include "manager.hpp"
#include "bluetooth.hpp"
#include "connection.hpp"
#include "freebsd-network.hpp"
#include "gtkmm/enums.h"
#include "network.hpp"
#include "network/settings.hpp"
#include "sigc++/functors/mem_fun.h"
#include "vpn.hpp"
#include "wifi.hpp"
#include "modem.hpp"
#include "wired.hpp"
#include "null.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define MM_DBUS_NAME "org.freedesktop.ModemManager1"
#define STRENGTH "Strength"

#define NM_PATH "/org/freedesktop/NetworkManager"

#define DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define NM_INTERFACE "org.freedesktop.NetworkManager"

#define ETHERNET_TYPE  1
#define WIFI_TYPE      2
#define BLUETOOTH_TYPE 5
#define MODEM_TYPE     8
#define GENERIC_TYPE   14
#define TUN_TYPE       16
#define WIREGUARD_TYPE 29
#define LOOPBACK_TYPE  32

NetworkManager::NetworkManager()
{
    popup_window.set_child(popup_box);
    popup_box.append(popup_label);
    popup_box.append(popup_entry);
    popup_box.set_orientation(Gtk::Orientation::VERTICAL);
    popup_entry.set_visibility(false);
    our_signals.push_back(popup_entry.signal_activate().connect(
        sigc::mem_fun(*this, &NetworkManager::submit_password)));

#ifdef __FreeBSD__
    /* ── FreeBSD: use ifconfig polling instead of NetworkManager ── */
    all_devices.emplace("/", new NullNetwork());
    connect_freebsd();
    nm_start.emit();
#else
    /* ── Linux / other: use NetworkManager over D-Bus ── */
    Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::SYSTEM,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        [this] (const Glib::RefPtr<Gio::AsyncResult> & result)
    {
        // Got a dbus proxy
        manager_proxy = Gio::DBus::Proxy::create_finish(result);
        auto val = manager_proxy->call_sync("ListNames");
        Glib::Variant<std::vector<std::string>> list;
        val.get_child(list, 0);
        auto l2 = list.get();
        for (auto t : l2)
        {
            if (t == NM_DBUS_NAME)
            {
                connect_nm();
            }

            if (t == MM_DBUS_NAME)
            {
                mm_start.emit();
            }
        }

        /* https://dbus.freedesktop.org/doc/dbus-java/api/org/freedesktop/DBus.NameOwnerChanged.html */
        our_signals.push_back(manager_proxy->signal_signal().connect(
            [this] (const Glib::ustring & sender_name,
                    const Glib::ustring & signal_name,
                    const Glib::VariantContainerBase & params)
        {
            if (signal_name == "NameOwnerChanged")
            {
                Glib::Variant<std::string> to, from, name;
                params.get_child(name, 0);
                params.get_child(from, 1);
                params.get_child(to, 2);
                if (name.get() == NM_DBUS_NAME)
                {
                    if (from.get() == "")
                    {
                        connect_nm();
                    } else if (to.get() == "")
                    {
                        lost_nm();
                    }
                } else if (name.get() == MM_DBUS_NAME)
                {
                    if (from.get() == "")
                    {
                        mm_start.emit();
                    } else if (to.get() == "")
                    {
                        for_each(all_devices.cbegin(), all_devices.cend(),
                            [this] (std::map<std::string, std::shared_ptr<Network>>::const_reference it)
                        {
                            if (std::dynamic_pointer_cast<ModemNetwork>(it.second) != nullptr)
                            {
                                device_removed.emit(it.second);
                                all_devices.erase(it.first);
                            }
                        });
                        mm_stop.emit();
                    }
                }
            }
        }));
    });
#endif
}

void NetworkManager::setting_added(std::string path)
{
    Gio::DBus::Proxy::create(
        connection,
        NM_DBUS_NAME,
        path,
        "org.freedesktop.NetworkManager.Settings.Connection", [=] (Glib::RefPtr<Gio::AsyncResult> & result)
    {
        auto proxy = Gio::DBus::Proxy::create_finish(result);
        all_settings.emplace(path,
            new NetworkSettings(path, proxy));
        auto setting = all_settings[path];

        if (setting->get_ssid() != "")
        {
            for (auto & device : all_devices)
            {
                auto wifi = std::dynamic_pointer_cast<WifiNetwork>(device.second);
                if (wifi)
                {
                    for (auto & ap : wifi->all_access_points)
                    {
                        if (ap.second && (ap.second->get_ssid() == setting->get_ssid()))
                        {
                            ap.second->set_has_saved_password(true);
                        }
                    }
                }
            }
        }
    });
}

void NetworkManager::setting_removed(std::string path)
{
    auto setting = all_settings[path];
    if (setting)
    {
        for (auto & device : all_devices)
        {
            auto wifi = std::dynamic_pointer_cast<WifiNetwork>(device.second);
            if (wifi)
            {
                for (auto & ap : wifi->all_access_points)
                {
                    if (ap.second->get_ssid() == setting->get_ssid())
                    {
                        ap.second->set_has_saved_password(false);
                    }
                }
            }
        }
    }

    all_settings.erase(path);
}

void NetworkManager::lost_nm()
{
    if (primary_signal)
    {
        primary_signal.disconnect();
    }

    if (debounce)
    {
        debounce.disconnect();
    }

    std::cout << "NetworkManager Lost" << std::endl;
    connection     = nullptr;
    settings_proxy = nullptr;
    nm_proxy = nullptr;
    for (auto & it : all_devices)
    {
        device_removed.emit(it.second);
    }

    all_devices.clear();
    all_vpns.clear();
    primary_connection     = "/";
    primary_connection_obj = std::make_shared<Connection>();
    for (auto signal : nm_dbus_signals)
    {
        signal.disconnect();
    }

    nm_stop.emit();
}

void NetworkManager::connect_nm()
{
    std::cout << "NetworkManager Found" << std::endl;
    all_devices.emplace("/", new NullNetwork());
    connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM);
    if (!connection)
    {
        std::cerr << "Failed to connect to dbus" << std::endl;
        return;
    }

    /* Get known VPNs */
    settings_proxy = Gio::DBus::Proxy::create_sync(connection,
        NM_DBUS_NAME,
        "/org/freedesktop/NetworkManager/Settings",
        "org.freedesktop.NetworkManager.Settings");
    if (!settings_proxy)
    {
        std::cerr << "No NM Settings proxy" << std::endl;
        return;
    }

    auto ret1 = settings_proxy->call_sync("ListConnections").get_child();
    Glib::Variant<std::vector<std::string>> ret =
        Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<std::string>>>(ret1);
    for (auto & it : ret.get())
    {
        setting_added(it);
        check_add_vpn(it);
    }

    nm_dbus_signals.push_back(settings_proxy->signal_signal().connect(
        [this] (const Glib::ustring& sender, const Glib::ustring& signal,
                const Glib::VariantContainerBase& container)
    {
        if (signal == "ConnectionRemoved")
        {
            auto var =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(container.get_child()).get();
            all_settings.erase(var);
            all_vpns.erase(var);
            vpn_removed.emit(var);
        } else if (signal == "NewConnection")
        {
            auto var =
                Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(container.get_child()).get();
            setting_added(var);
            check_add_vpn(var);
        }
    }));

    nm_proxy = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME,
        NM_PATH,
        NM_INTERFACE);
    if (!nm_proxy)
    {
        std::cerr << "Failed to connect to network manager, " <<
            "are you sure it is running?" << std::endl;
        return;
    }

    nm_dbus_signals.push_back(nm_proxy->signal_properties_changed().connect(
        sigc::mem_fun(*this, &NetworkManager::on_nm_properties_changed)));

    nm_dbus_signals.push_back(nm_proxy->signal_signal().connect(
        sigc::mem_fun(*this, &NetworkManager::on_nm_signal)));

    /* Fill Initial List*/

    nm_proxy->call("GetAllDevices", sigc::mem_fun(*this, &NetworkManager::get_all_devices_cb));
}

void NetworkManager::check_add_vpn(std::string path)
{
    auto proxy = Gio::DBus::Proxy::create_sync(connection,
        NM_DBUS_NAME,
        path,
        "org.freedesktop.NetworkManager.Settings.Connection");

    auto values = Glib::VariantBase::cast_dynamic<Glib::Variant<std::map<std::string, std::map<std::string,
        Glib::VariantBase>>>>(proxy->call_sync("GetSettings").get_child());
    auto hash = values.get();
    if ((hash.count("connection") == 1) && (hash["connection"].count("id") == 1) &&
        (hash["connection"].count("type") == 1))
    {
        auto contype = hash["connection"]["type"];
        auto conname = hash["connection"]["id"];
        auto strtype = Glib::VariantType("s");
        if (contype.is_of_type(strtype) && conname.is_of_type(strtype))
        {
            auto name = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(conname).get();
            auto contype_str = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(contype).get();
            if ((contype_str == "vpn") || (contype_str == "wireguard"))
            {
                all_vpns[path] = std::make_shared<VpnConfig>(path, proxy, name);
                vpn_added.emit(all_vpns[path]);
            }
        } else
        {
            std::cerr << "INVALID TYPES " << conname.get_type_string() << " " << contype.get_type_string() <<
                std::endl;
        }
    }
}

void NetworkManager::get_all_devices_cb(std::shared_ptr<Gio::AsyncResult> async)
{
    auto list = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<std::string>>>(nm_proxy->call_finish(
        async).get_child()).get();
    for (auto & val : list)
    {
        add_network(val);
    }

    /* Now get the current connection */
    nm_dbus_signals.push_back(Glib::signal_idle().connect([this] ()
    {
        changed_primary();
        return G_SOURCE_REMOVE;
    }));
    /* And emit a start event */
    nm_start.emit();
}

void NetworkManager::add_network(std::string path)
{
    Glib::RefPtr<Gio::DBus::Proxy> device_proxy = Gio::DBus::Proxy::create_sync(connection,
        NM_DBUS_NAME,
        path,
        "org.freedesktop.NetworkManager.Device");
    Glib::Variant<uint> type;
    device_proxy->get_cached_property(type, "DeviceType");
    uint connection_type = type.get();
    if (connection_type == WIFI_TYPE)
    {
        Gio::DBus::Proxy::create(connection,
            NM_DBUS_NAME,
            path,
            "org.freedesktop.NetworkManager.Device.Wireless",
            [=] (Glib::RefPtr<Gio::AsyncResult> & result)
        {
            auto wifi_proxy = Gio::DBus::Proxy::create_finish(result);
            all_devices.emplace(path, new WifiNetwork(path, device_proxy, wifi_proxy));
            device_added.emit(all_devices[path]);
        });

        return;
    } else if (connection_type == MODEM_TYPE)
    {
        Gio::DBus::Proxy::create(connection,
            NM_DBUS_NAME,
            path,
            "org.freedesktop.NetworkManager.Device.Modem",
            [=] (Glib::RefPtr<Gio::AsyncResult> & result)
        {
            auto modem_proxy = Gio::DBus::Proxy::create_finish(result);
            all_devices.emplace(path, new ModemNetwork(path, device_proxy, modem_proxy));
            device_added.emit(all_devices[path]);
        });

        return;
    } else if (connection_type == BLUETOOTH_TYPE)
    {
        Gio::DBus::Proxy::create(connection,
            NM_DBUS_NAME,
            path,
            "org.freedesktop.NetworkManager.Device.Bluetooth", [=] (Glib::RefPtr<Gio::AsyncResult> & result)
        {
            auto bluetooth_proxy = Gio::DBus::Proxy::create_finish(result);
            all_devices.emplace(path, new BluetoothNetwork(path, device_proxy, bluetooth_proxy));
            device_added.emit(all_devices[path]);
        });
        return;
    } else if (connection_type == ETHERNET_TYPE)
    {
        all_devices.emplace(path, new WiredNetwork(path, device_proxy));
        device_added.emit(all_devices[path]);
        return;
    } else if ((connection_type == LOOPBACK_TYPE) ||
               (connection_type == TUN_TYPE))
    {
        /* Known and ignored
         *  Loopback - nothing for user to do or know about this
         *  TUN - Part of VPN systems. Either controlled by VPN Settings or ignore entirely
         */
        return;
    }

    std::cerr << "DeviceAdded for unknown device type : " << path << " : type : " << connection_type <<
        std::endl;
}

void NetworkManager::on_nm_properties_changed(const Gio::DBus::Proxy::MapChangedProperties& properties,
    const std::vector<Glib::ustring>& invalidated)
{
    for (auto & it : properties)
    {
        if ((it.first == "PrimaryConnection") ||
            (it.first == "ActiveConnections"))
        {
            changed_primary();
        } else if ((it.first == "NetworkingEnabled") || (it.first == "WirelessEnabled") ||
                   (it.first == "WirelessHardwareEnabled") ||
                   (it.first == "WwanEnabled") || (it.first == "WwanHardwareEnabled"))
        {
            global_change.emit();
        }
    }
}

void NetworkManager::changed_primary()
{
    Glib::Variant<std::string> primary_value, connecting_value;
    Glib::Variant<std::vector<std::string>> connections_value;
    nm_proxy->get_cached_property(primary_value, "PrimaryConnection");
    nm_proxy->get_cached_property(connections_value, "ActiveConnections");

    auto connection_path = primary_value.get();

    std::shared_ptr<Connection> network;
    /* At this point there are two known paths for VPNS
     * OpenVPN shows up as primary active connection pointing to the device under it
     * WireGuard shows up as 'an' active connection but not primary
     */

    auto connections_values = connections_value.get();

    auto vpns_not_changed{all_vpns};
    for (auto connection_path_loop : connections_values)
    {
        /* Pile up wireguard connections */
        auto maybe_wireguard_connection = get_connection(connection_path_loop);
        if (maybe_wireguard_connection->has_wireguard)
        {
            auto next_step = get_connection(connection_path);
            maybe_wireguard_connection->replace_devices(next_step->devices);
            connection_path = connection_path_loop;
            network = maybe_wireguard_connection;
        }

        auto connection_proxy = Gio::DBus::Proxy::create_sync(connection,
            NM_DBUS_NAME,
            connection_path_loop,
            "org.freedesktop.NetworkManager.Connection.Active");

        Glib::Variant<std::string> inner_connection_val;
        connection_proxy->get_cached_property(inner_connection_val, "Connection");
        if (inner_connection_val && (vpns_not_changed.count(inner_connection_val.get()) > 0))
        {
            auto vpn = vpns_not_changed[inner_connection_val.get()];
            vpn->set_connection_path(connection_path_loop);
            vpn->set_active(true);
            vpns_not_changed.erase(inner_connection_val.get());
        }
    }

    for (auto & it : vpns_not_changed)
    {
        it.second->set_active(false);
    }

    /* Different than previously, reattach primary */
    if (connection_path != primary_connection)
    {
        if (primary_signal)
        {
            primary_signal.disconnect();
        }

        primary_connection = connection_path;
        if (!network)
        {
            network = get_connection(connection_path);
        }

        assert(network != nullptr);
        primary_connection_obj = network;

        /* Any change inside the primary connection also called default_changed */
        primary_signal = network->signal_network_altered().connect(
            [this, network] ()
        {
            default_changed.emit(network);
        });
        /* Tell clients */
        default_changed.emit(network);
    }
}

std::shared_ptr<Connection> NetworkManager::get_connection(std::string path)
{
    auto connection_proxy = Gio::DBus::Proxy::create_sync(connection,
        NM_DBUS_NAME,
        path,
        "org.freedesktop.NetworkManager.Connection.Active");

    std::vector<std::shared_ptr<Network>> list;
    Glib::Variant<std::vector<Glib::ustring>> paths;
    connection_proxy->get_cached_property(paths, "Devices");
    if (paths)
    {
        for (auto & it : paths.get())
        {
            list.push_back(all_devices[it]);
        }
    }

    return std::make_shared<Connection>(path, connection_proxy, list);
}

void NetworkManager::on_nm_signal(const Glib::ustring& sender, const Glib::ustring& signal,
    const Glib::VariantContainerBase& container)
{
    if (signal == "DeviceAdded")
    {
        auto val = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(container.get_child()).get();
        add_network(val);
    } else if (signal == "DeviceRemoved")
    {
        auto val =
            Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(container.get_child()).get();
        auto network = all_devices[val];
        if (network)
        {
            device_removed.emit(network);
        } else
        {
            /* Skip networks we avoided dealing with */
            std::cerr << "DeviceRemoved did not exist : " << val << std::endl;
        }

        all_devices.erase(val);
    } else
    {
        return;
    }

    /* NM list changed, but let's not send instantly */
    if (debounce)
    {
        debounce.disconnect();
    }

    debounce = Glib::signal_timeout().connect([this] ()
    {
        signal_device_list_changed().emit(all_devices);
        return G_SOURCE_REMOVE;
    }, 100);
}

std::shared_ptr<Connection> NetworkManager::get_primary_network()
{
    return primary_connection_obj;
}

void NetworkManager::activate_connection(std::string p1, std::string p2, std::string p3)
{
    Glib::VariantStringBase path1, path2, path3;
    Glib::VariantStringBase::create_object_path(path1, p1);
    Glib::VariantStringBase::create_object_path(path2, p2);
    Glib::VariantStringBase::create_object_path(path3, p3);
    auto paths = Glib::VariantContainerBase::create_tuple({path1, path2, path3});

    try {
        nm_proxy->call("ActivateConnection", paths);
    } catch (...)
    {}
}

void NetworkManager::request_password(std::string device_path, std::string ap_path)
{
    if (device_path.find("/Devices/") != std::string::npos)
    {
        auto device = std::dynamic_pointer_cast<WifiNetwork>(all_devices[device_path]);
        if (device)
        {
            popup_cache_device = device_path;
            popup_cache_ap     = ap_path;
            auto ap = device->get_access_points()[ap_path];
            popup_label.set_label("Preshared Key required for Access Point '" + ap->get_ssid() + "'");
            popup_window.present();
            popup_window.get_focus();
        }
    }
}

void NetworkManager::deactivate_connection(std::string p1)
{
    auto manager = NetworkManager::getInstance()->get_nm_proxy();
    Glib::VariantStringBase path1;
    Glib::VariantStringBase::create_object_path(path1, p1);
    auto paths = Glib::VariantContainerBase::create_tuple({path1});
    // auto data = Glib::VariantContainerBase::create_tuple(paths);

    try {
        manager->call("DeactivateConnection", paths);
    } catch (...)
    {}
}

/* Is Wifi Enabled, in software and in rfkill */
std::tuple<bool, bool> NetworkManager::wifi_global_enabled()
{
    if (!nm_proxy)
    {
        return {false, false};
    }

    Glib::Variant<bool> wifisoft, wifihard;
    nm_proxy->get_cached_property(wifisoft, "WirelessEnabled");
    nm_proxy->get_cached_property(wifihard, "WirelessHardwareEnabled");
    return {wifisoft.get(), wifihard.get()};
}

/* Is Mobile Data Enabled, in software and in rfkill */
std::tuple<bool, bool> NetworkManager::mobile_global_enabled()
{
    if (!nm_proxy)
    {
        return {false, false};
    }

    Glib::Variant<bool> modemsoft, modemhard;
    nm_proxy->get_cached_property(modemsoft, "WwanEnabled");
    nm_proxy->get_cached_property(modemhard, "WwanHardwareEnabled");
    return {modemsoft.get(), modemhard.get()};
}

/* Is Networking enabled */
bool NetworkManager::networking_global_enabled()
{
    if (!nm_proxy)
    {
        return false;
    }

    Glib::Variant<bool> enabled;
    nm_proxy->get_cached_property(enabled, "NetworkingEnabled");
    return enabled.get();
}

void NetworkManager::wifi_global_set(bool value)
{
    Gio::DBus::Proxy::create(nm_proxy->get_connection(),
        NM_DBUS_NAME,
        NM_PATH,
        DBUS_PROPERTIES_INTERFACE,
        [=] (Glib::RefPtr<Gio::AsyncResult>& result)
    {
        auto another_proxy = Gio::DBus::Proxy::create_finish(result);
        auto params = Glib::VariantContainerBase::create_tuple(
        {
            Glib::Variant<Glib::ustring>::create(NM_INTERFACE),
            Glib::Variant<Glib::ustring>::create("WirelessEnabled"),
            Glib::Variant<Glib::VariantBase>::create(Glib::Variant<bool>::create(value))
        });
        another_proxy->call("Set", params);
    });
}

void NetworkManager::mobile_global_set(bool value)
{
    Gio::DBus::Proxy::create(nm_proxy->get_connection(),
        NM_DBUS_NAME,
        NM_PATH,
        DBUS_PROPERTIES_INTERFACE,
        [=] (Glib::RefPtr<Gio::AsyncResult> & result)
    {
        auto another_proxy = Gio::DBus::Proxy::create_finish(result);
        auto params = Glib::VariantContainerBase::create_tuple(
        {
            Glib::Variant<Glib::ustring>::create(NM_INTERFACE),
            Glib::Variant<Glib::ustring>::create("WwanEnabled"),

            Glib::Variant<Glib::VariantBase>::create(Glib::Variant<bool>::create(value))
        });
        another_proxy->call("Set", params);
    });
}

std::shared_ptr<NetworkSettings> NetworkManager::get_setting_for_ssid(std::string ssid)
{
    if (ssid == "")
    {
        return nullptr;
    }

    for (auto setting : all_settings)
    {
        if (setting.second->get_ssid() == ssid)
        {
            return setting.second;
        }
    }

    return nullptr;
}

std::shared_ptr<VpnConfig> NetworkManager::get_vpn(std::string path)
{
    return all_vpns[path];
}

void NetworkManager::submit_password()
{
    auto password = popup_entry.get_text();
    if (password.length() == 0)
    {
        return;
    }

    popup_entry.set_text("");
    auto wifi = std::dynamic_pointer_cast<WifiNetwork>(all_devices[popup_cache_device]);
    if (!wifi)
    {
        return;
    }

    auto ap = wifi->get_access_points()[popup_cache_ap];
    if (!ap)
    {
        return;
    }

    auto ssid = ap->get_ssid();
    if (ssid.length() == 0)
    {
        return;
    }

    popup_window.hide();

    // --- Build settings using glibmm (correct types) ---

    // SSID as byte array 'ay'
    std::vector<guint8> ssid_bytes(ssid.begin(), ssid.end());
    auto ssid_variant = Glib::Variant<std::vector<guint8>>::create(ssid_bytes);

    // UUID
    gchar *uuid_c = g_uuid_string_random();
    Glib::ustring uuid(uuid_c);
    g_free(uuid_c);

    // ----- connection (a{sv})
    std::map<Glib::ustring, Glib::VariantBase> connection_map;
    connection_map["type"] =
        Glib::Variant<Glib::ustring>::create("802-11-wireless");
    connection_map["id"]   = Glib::Variant<Glib::ustring>::create(ssid);
    connection_map["uuid"] = Glib::Variant<Glib::ustring>::create(uuid);

    // ----- 802-11-wireless (a{sv})
    std::map<Glib::ustring, Glib::VariantBase> wifi_map;
    wifi_map["ssid"] = ssid_variant;
    wifi_map["mode"] = Glib::Variant<Glib::ustring>::create("infrastructure");

    // ----- 802-11-wireless-security (a{sv})
    std::map<Glib::ustring, Glib::VariantBase> sec_map;
    bool use_security = !password.empty();
    if (use_security)
    {
        sec_map["key-mgmt"] = Glib::Variant<Glib::ustring>::create("wpa-psk");
        sec_map["psk"] = Glib::Variant<Glib::ustring>::create(password);
    }

    // ------------------------
    // TOP-LEVEL SETTINGS (a{sa{sv}})
    // ------------------------
    std::map<Glib::ustring, std::map<Glib::ustring, Glib::VariantBase>>
    settings_map;

    settings_map["connection"] = connection_map;
    settings_map["802-11-wireless"] = wifi_map;
    if (use_security)
    {
        settings_map["802-11-wireless-security"] = sec_map;
    }

    auto settings = Glib::Variant<
        std::map<Glib::ustring, std::map<Glib::ustring, Glib::VariantBase>>>::
        create(settings_map);
    // ------------------------
    // Object paths (o)
    // ------------------------
    auto device_path =
        Glib::Variant<Glib::DBusObjectPathString>::create(popup_cache_device);
    // Access point path is "/" → NM autoselects AP matching SSID
    auto ap_path = Glib::Variant<Glib::DBusObjectPathString>::create("/");
    // ------------------------
    // FINAL TUPLE (a{sa{sv}}, o, o)
    // ------------------------
    std::vector<Glib::VariantBase> args_vec = {settings, device_path, ap_path};
    auto args = Glib::VariantContainerBase::create_tuple(args_vec);

    // ------------------------
    // CALL NetworkManager
    // ------------------------
    nm_proxy->call("AddAndActivateConnection", args);
}

void NetworkManager::networking_global_set(bool value)
{
    auto params = Glib::VariantContainerBase::create_tuple(Glib::Variant<bool>::create(value));
    nm_proxy->call("Enable", params);
}

/* ─── FreeBSD backend ─────────────────────────────────────────────────────── */

#ifdef __FreeBSD__

/*
 * Detect whether an interface is a wireless interface on FreeBSD.
 * Wireless interfaces are typically wlanN, which is a virtual interface
 * layered over a physical device (e.g. iwm0, iwn0, urtwn0).
 * The actual physical interface does not expose Wi-Fi scan capabilities.
 */
static bool is_wireless_iface(const char *iface)
{
    /* wlanN is the FreeBSD Wi-Fi virtual interface */
    return (strncmp(iface, "wlan", 4) == 0) && (iface[4] >= '0' && iface[4] <= '9');
}

void NetworkManager::refresh_freebsd_devices()
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        return;
    }

    std::map<std::string, std::string> current; // interface → path

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        /* Only consider AF_INET (IPv4) addresses — they're present on any up interface.
         * We skip loopback (lo0). */
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (strncmp(ifa->ifa_name, "lo", 2) == 0) {
            continue;
        }

        std::string iface = ifa->ifa_name;
        std::string path = "/org/freedesktop/NetworkManager/Devices/freebsd/" + iface;

        current[iface] = path;

        if (all_devices.find(path) == all_devices.end()) {
            /* New interface */
            bool wireless = is_wireless_iface(iface.c_str());
            auto net = std::make_shared<FreeBSDNetwork>(path, iface, wireless);
            all_devices.emplace(path, net);
            device_added.emit(all_devices[path]);
        }
    }

    freeifaddrs(ifaddr);

    /* Remove interfaces that have disappeared */
    std::vector<std::string> removed;
    for (const auto& [path, dev] : all_devices) {
        if (path == "/") {
            continue; // NullNetwork sentinel
        }
        // Extract interface name from path
        std::string iface = path.substr(path.rfind('/') + 1);
        if (current.find(iface) == current.end()) {
            removed.push_back(path);
        }
    }
    for (const auto& path : removed) {
        device_removed.emit(all_devices[path]);
        all_devices.erase(path);
    }

    /* Re-emit network altered on each device to refresh icon state */
    for (auto& [path, dev] : all_devices) {
        if (path != "/") {
            dev->signal_network_altered().emit();
        }
    }
}

void NetworkManager::connect_freebsd()
{
    /* Initial scan */
    refresh_freebsd_devices();

    /* Poll every 3 seconds for interface changes */
    freebsd_poll = Glib::signal_timeout().connect(
        [this]() -> bool {
            refresh_freebsd_devices();
            return true;
        },
        3000);
}

#endif /* __FreeBSD__ */

NetworkManager::~NetworkManager()
{
#ifdef __FreeBSD__
    if (freebsd_poll.connected()) {
        freebsd_poll.disconnect();
    }
#endif
    lost_nm();
    /* Signals that outlast each NetworkManager start/stop but need to clear on widget reload */
    for (auto signal : our_signals)
    {
        signal.disconnect();
    }
}
