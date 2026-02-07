#include <giomm.h>
#include <glibmm.h>
#include <iostream>
#include <memory>
#include <string>

#include "manager.hpp"
#include "bluetooth.hpp"
#include "connection.hpp"
#include "gtkmm/enums.h"
#include "network.hpp"
#include "network/settings.hpp"
#include "sigc++/functors/mem_fun.h"
#include "vpn.hpp"
#include "wifi.hpp"
#include "modem.hpp"
#include "wired.hpp"
#include "null.hpp"
#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define MM_DBUS_NAME "org.freedesktop.ModemManager1"
#define ACTIVE_CONNECTION "PrimaryConnection"
#define STRENGTH "Strength"

#define NM_PATH "/org/freedesktop/NetworkManager"

#define DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define NM_INTERFACE "org.freedesktop.NetworkManager"

#define ETHERNET_TYPE  1
#define WIFI_TYPE      2
#define MODEM_TYPE     8
#define GENERIC_TYPE   14
#define BLUETOOTH_TYPE 5

NetworkManager::NetworkManager()
{
    popup_window.set_child(popup_box);
    popup_box.append(popup_label);
    popup_box.append(popup_entry);
    popup_box.set_orientation(Gtk::Orientation::VERTICAL);
    popup_entry.set_visibility(false);
    our_signals.push_back(popup_entry.signal_activate().connect(
        sigc::mem_fun(*this, &NetworkManager::submit_password)));
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
}

void NetworkManager::setting_added(std::string path)
{
    auto proxy = Gio::DBus::Proxy::create_sync(
        connection,
        NM_DBUS_NAME,
        path,
        "org.freedesktop.NetworkManager.Settings.Connection");

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
        vpn_added.emit(it);
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
            vpn_added.emit(var);
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
            if (contype_str == "vpn")
            {
                all_vpns[path] = std::make_shared<VpnConfig>(path, proxy, name);
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
        Glib::Variant<std::string> path_read;
        nm_proxy->get_cached_property(path_read, "PrimaryConnection");
        changed_primary(path_read.get());
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
        auto wifi_proxy = Gio::DBus::Proxy::create_sync(connection,
            NM_DBUS_NAME,
            path,
            "org.freedesktop.NetworkManager.Device.Wireless");
        all_devices.emplace(path, new WifiNetwork(path, device_proxy, wifi_proxy));
        device_added.emit(all_devices[path]);
    } else if (connection_type == MODEM_TYPE)
    {
        auto modem_proxy = Gio::DBus::Proxy::create_sync(connection,
            NM_DBUS_NAME,
            path,
            "org.freedesktop.NetworkManager.Device.Modem");
        all_devices.emplace(path, new ModemNetwork(path, device_proxy, modem_proxy));
        device_added.emit(all_devices[path]);
    } else if (connection_type == BLUETOOTH_TYPE)
    {
        auto bluetooth_proxy = Gio::DBus::Proxy::create_sync(connection,
            NM_DBUS_NAME,
            path,
            "org.freedesktop.NetworkManager.Device.Bluetooth");
        all_devices.emplace(path, new BluetoothNetwork(path, device_proxy, bluetooth_proxy));
        device_added.emit(all_devices[path]);
    } else if (connection_type == ETHERNET_TYPE)
    {
        all_devices.emplace(path, new WiredNetwork(path, device_proxy));
        device_added.emit(all_devices[path]);
    }
}

void NetworkManager::on_nm_properties_changed(const Gio::DBus::Proxy::MapChangedProperties& properties,
    const std::vector<Glib::ustring>& invalidated)
{
    for (auto & it : properties)
    {
        if (it.first == "PrimaryConnection")
        {
            auto value = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(it.second).get();
            changed_primary(value);
        } else if ((it.first == "NetworkingEnabled") || (it.first == "WirelessEnabled") ||
                   (it.first == "WirelessHardwareEnabled") ||
                   (it.first == "WwanEnabled") || (it.first == "WwanHardwareEnabled"))
        {
            global_change.emit();
        }
    }
}

void NetworkManager::changed_primary(std::string value)
{
    if (value != primary_connection)
    {
        if (primary_signal)
        {
            primary_signal.disconnect();
        }

        primary_connection = value;
        auto network = get_connection(value);
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
        auto val = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string>>(container.get_child()).get();
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
    // auto data = Glib::VariantContainerBase::create_tuple(paths);

    try {
        nm_proxy->call_sync("ActivateConnection", paths);
    } catch (...)
    {
        /* It's most likely a WIFI AP with no password set. Let's ask */
        std::cout << p2 << std::endl;
        if (p2.find("/Devices/") != std::string::npos)
        {
            auto device = std::dynamic_pointer_cast<WifiNetwork>(all_devices[p2]);
            if (device)
            {
                popup_cache_p2 = p2;
                popup_cache_p3 = p3;
                auto ap = device->get_access_points()[p3];
                popup_label.set_label("Preshared Key required for Access Point '" + ap->get_ssid() + "'");
                popup_window.present();
                popup_window.get_focus();
            }
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
        manager->call_sync("DeactivateConnection", paths);
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
    auto another_proxy = Gio::DBus::Proxy::create_sync(nm_proxy->get_connection(),
        NM_DBUS_NAME,
        NM_PATH,
        DBUS_PROPERTIES_INTERFACE);

    auto params = Glib::VariantContainerBase::create_tuple(
    {
        Glib::Variant<Glib::ustring>::create(NM_INTERFACE),
        Glib::Variant<Glib::ustring>::create("WirelessEnabled"),
        Glib::Variant<Glib::VariantBase>::create(Glib::Variant<bool>::create(value))
    });
    another_proxy->call_sync("Set", params);
}

void NetworkManager::mobile_global_set(bool value)
{
    auto another_proxy = Gio::DBus::Proxy::create_sync(nm_proxy->get_connection(),
        NM_DBUS_NAME,
        NM_PATH,
        DBUS_PROPERTIES_INTERFACE);

    auto params = Glib::VariantContainerBase::create_tuple(
    {
        Glib::Variant<Glib::ustring>::create(NM_INTERFACE),
        Glib::Variant<Glib::ustring>::create("WwanEnabled"),

        Glib::Variant<Glib::VariantBase>::create(Glib::Variant<bool>::create(value))
    });
    another_proxy->call_sync("Set", params);
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
    auto wifi = std::dynamic_pointer_cast<WifiNetwork>(all_devices[popup_cache_p2]);
    if (!wifi)
    {
        return;
    }

    auto ap = wifi->get_access_points()[popup_cache_p3];
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
        Glib::Variant<Glib::DBusObjectPathString>::create(popup_cache_p2);
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
    nm_proxy->call_sync("AddAndActivateConnection", args);
}

void NetworkManager::networking_global_set(bool value)
{
    auto params = Glib::VariantContainerBase::create_tuple(Glib::Variant<bool>::create(value));
    nm_proxy->call_sync("Enable", params);
}

NetworkManager::~NetworkManager()
{
    lost_nm();
    /* Signals that outlast each NetworkManager start/stop but need to clear on widget reload */
    for (auto signal : our_signals)
    {
        signal.disconnect();
    }
}
