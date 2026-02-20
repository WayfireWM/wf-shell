#include "network.hpp"
#include "giomm/dbusconnection.h"
#include "giomm/dbusproxy.h"
#include "gtkmm/enums.h"
#include "lockergrid.hpp"
#include "plugin.hpp"
#include "timedrevealer.hpp"
#include <memory>
#include <iostream>

#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define ACTIVE_CONNECTION "PrimaryConnection"
#define STRENGTH "Strength"

/* Connection types */
struct NoConnectionInfo : public WfPluginNetworkConnectionInfo
{
    std::string get_icon_name(WfPluginConnectionState state)
    {
        return "network-offline-symbolic";
    }

    int get_connection_strength()
    {
        return 0;
    }

    std::string get_strength_str()
    {
        return "none";
    }

    std::string get_ip()
    {
        return "127.0.0.1";
    }

    virtual ~NoConnectionInfo()
    {}
};

struct VPNPluginConnectionInfo : public WfPluginNetworkConnectionInfo
{
    VPNPluginConnectionInfo(const std::shared_ptr<Gio::DBus::Connection>& connection, std::string path)
    {}
    virtual std::string get_icon_name(WfPluginConnectionState state) override
    {
        return "network-vpn-symbolic";
    }

    int get_connection_strength() override
    {
        return 0;
    }

    std::string get_strength_str() override
    {
        return "excellent";
    }

    std::string get_ip() override
    {
        return "0.0.0.0";
    }

    virtual ~VPNPluginConnectionInfo()
    {}
};


struct WifiPluginConnectionInfo : public WfPluginNetworkConnectionInfo
{
    WayfireLockerNetworkPlugin *plugin;
    std::shared_ptr<Gio::DBus::Proxy> ap;
    sigc::connection ap_sig;

    WifiPluginConnectionInfo(const std::shared_ptr<Gio::DBus::Connection>& connection, std::string path,
        WayfireLockerNetworkPlugin *plugin)
    {
        this->plugin = plugin;

        ap = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME, path,
            "org.freedesktop.NetworkManager.AccessPoint");

        if (ap)
        {
            ap_sig = ap->signal_properties_changed().connect(
                sigc::mem_fun(*this, &WifiPluginConnectionInfo::on_properties_changed));
        }
    }

    void on_properties_changed(const Gio::DBus::Proxy::MapChangedProperties& changed,
        const std::vector<Glib::ustring>& invalid)
    {
        bool needs_refresh = false;
        for (auto& prop : changed)
        {
            if (prop.first == STRENGTH)
            {
                needs_refresh = true;
            }
        }

        if (needs_refresh)
        {
            plugin->set_state();
        }
    }

    int get_strength()
    {
        assert(ap);

        Glib::Variant<guchar> vstr;
        ap->get_cached_property(vstr, STRENGTH);

        return vstr.get();
    }

    std::string get_strength_str()
    {
        int value = get_strength();

        if (value > 80)
        {
            return "excellent";
        }

        if (value > 55)
        {
            return "good";
        }

        if (value > 30)
        {
            return "ok";
        }

        if (value > 5)
        {
            return "weak";
        }

        return "none";
    }

    virtual std::string get_icon_name(WfPluginConnectionState state)
    {
        if ((state <= CSTATE_ACTIVATING) || (state == CSTATE_DEACTIVATING))
        {
            return "network-wireless-acquiring-symbolic";
        }

        if (state == CSTATE_DEACTIVATED)
        {
            return "network-wireless-disconnected-symbolic";
        }

        if (ap)
        {
            return "network-wireless-signal-" + get_strength_str() + "-symbolic";
        } else
        {
            return "network-wireless-no-route-symbolic";
        }
    }

    virtual int get_connection_strength()
    {
        if (ap)
        {
            return get_strength();
        } else
        {
            return 100;
        }
    }

    virtual std::string get_ip()
    {
        return "0.0.0.0";
    }

    virtual ~WifiPluginConnectionInfo()
    {}
};

struct EthernetPluginConnectionInfo : public WfPluginNetworkConnectionInfo
{
    std::shared_ptr<Gio::DBus::Proxy> ap;
    EthernetPluginConnectionInfo(const std::shared_ptr<Gio::DBus::Connection>& connection, std::string path)
    {}

    virtual std::string get_icon_name(WfPluginConnectionState state)
    {
        if ((state <= CSTATE_ACTIVATING) || (state == CSTATE_DEACTIVATING))
        {
            return "network-wired-acquiring-symbolic";
        }

        if (state == CSTATE_DEACTIVATED)
        {
            return "network-wired-disconnected-symbolic";
        }

        return "network-wired-symbolic";
    }

    std::string get_connection_name()
    {
        return "Ethernet - " + connection_name;
    }

    std::string get_strength_str()
    {
        return "excellent";
    }

    virtual int get_connection_strength()
    {
        return 100;
    }

    virtual std::string get_ip()
    {
        return "0.0.0.0";
    }

    virtual ~EthernetPluginConnectionInfo()
    {}
};

/* Main plugin */

WayfireLockerNetworkPluginWidget::WayfireLockerNetworkPluginWidget(std::string image_contents,
    std::string label_contents,
    std::string css_contents) :
    WayfireLockerTimedRevealer("locker/network_always")
{
    box.add_css_class("network");
    box.append(image);
    box.append(label);
    image.set_from_icon_name(image_contents);
    label.set_label(label_contents);
    label.add_css_class(css_contents);
    box.set_orientation(Gtk::Orientation::HORIZONTAL);
    set_child(box);
}

WayfireLockerNetworkPlugin::WayfireLockerNetworkPlugin() :
    WayfireLockerPlugin("locker/network")
{}

void WayfireLockerNetworkPlugin::init()
{
    setup_dbus();
}

void WayfireLockerNetworkPlugin::deinit()
{}

void WayfireLockerNetworkPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerNetworkPluginWidget(image_contents, label_contents, css_contents));
    auto widget = widgets[id];
    /* Add to window */
    grid->attach(*widget, position);
    update_active_connection();
}

void WayfireLockerNetworkPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.erase(id);
}

bool WayfireLockerNetworkPlugin::setup_dbus()
{
    auto cancellable = Gio::Cancellable::create();
    connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM, cancellable);
    if (!connection)
    {
        std::cerr << "Failed to connect to dbus" << std::endl;
        return false;
    }

    nm_proxy = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME,
        "/org/freedesktop/NetworkManager",
        "org.freedesktop.NetworkManager");
    if (!nm_proxy)
    {
        std::cerr << "Failed to connect to network manager, " <<
            "are you sure it is running?" << std::endl;
        return false;
    }

    signals.push_back(nm_proxy->signal_properties_changed().connect(
        sigc::mem_fun(*this, &WayfireLockerNetworkPlugin::on_nm_properties_changed)));

    return true;
}

void WayfireLockerNetworkPlugin::on_nm_properties_changed(
    const Gio::DBus::Proxy::MapChangedProperties& properties,
    const std::vector<Glib::ustring>& invalidated)
{
    for (auto & prop : properties)
    {
        if (prop.first == ACTIVE_CONNECTION)
        {
            update_active_connection();
        }
    }
}

void WayfireLockerNetworkPlugin::update_active_connection()
{
    std::cout << "Updating connection " << std::endl;
    Glib::Variant<Glib::ustring> active_conn_path;
    nm_proxy->get_cached_property(active_conn_path, ACTIVE_CONNECTION);

    if (active_conn_path && (active_conn_path.get() != "/"))
    {
        active_connection_proxy = Gio::DBus::Proxy::create_sync(
            connection, NM_DBUS_NAME, active_conn_path.get(),
            "org.freedesktop.NetworkManager.Connection.Active");
    } else
    {
        active_connection_proxy = nullptr;
    }

    if (!active_connection_proxy)
    {
        info = std::make_unique<NoConnectionInfo>();
        set_state();
        return;
    }

    Glib::Variant<Glib::ustring> vtype, vobject;
    active_connection_proxy->get_cached_property(vtype, "Type");
    active_connection_proxy->get_cached_property(vobject, "SpecificObject");
    auto type   = vtype.get();
    auto object = vobject.get();

    if (type.find("wireless") != type.npos)
    {
        info = std::make_unique<WifiPluginConnectionInfo>(
            connection, object, this);
    } else if (type.find("ethernet") != type.npos)
    {
        info = std::make_unique<EthernetPluginConnectionInfo>(connection, object);
    } else if (type == "vpn")
    {
        info = std::make_unique<VPNPluginConnectionInfo>(connection, object);
    } else
    {
        info = std::make_unique<NoConnectionInfo>();
    }

    Glib::Variant<Glib::ustring> vname;
    active_connection_proxy->get_cached_property(vname, "Id");
    info->connection_name = vname.get();
    set_state();
}

static WfPluginConnectionState get_connection_state(std::shared_ptr<Gio::DBus::Proxy> connection)
{
    if (!connection)
    {
        return CSTATE_DEACTIVATED;
    }

    Glib::Variant<guint32> state;
    connection->get_cached_property(state, "State");
    return (WfPluginConnectionState)state.get();
}

void WayfireLockerNetworkPlugin::set_state()
{
    label_contents = info->get_connection_name();
    image_contents = info->get_icon_name(get_connection_state(active_connection_proxy));
    css_contents   = info->get_strength_str();
    std::cout << label_contents << " " << image_contents << std::endl;

    for (auto & it : widgets)
    {
        it.second->label.remove_css_class("excellent");
        it.second->label.remove_css_class("good");
        it.second->label.remove_css_class("weak");
        it.second->label.remove_css_class("none");
        it.second->label.set_label(label_contents);
        it.second->image.set_from_icon_name(image_contents);
        it.second->label.add_css_class(css_contents);
    }
}
