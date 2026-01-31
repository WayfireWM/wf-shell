#include "network.hpp"
#include <glibmm/spawn.h>
#include <cassert>
#include <iostream>
#include <gtk-utils.hpp>

#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define ACTIVE_CONNECTION "PrimaryConnection"
#define STRENGTH "Strength"

std::string WfNetworkConnectionInfo::get_control_center_section(DBusProxy& nm)
{
    Glib::Variant<bool> wifi;
    nm->get_cached_property(wifi, "WirelessEnabled");

    return wifi.get() ? "wifi" : "network";
}

void WfNetworkConnectionInfo::spawn_control_center(DBusProxy& nm)
{
    std::string command = "env XDG_CURRENT_DESKTOP=GNOME gnome-control-center ";
    command += get_control_center_section(nm);

    Glib::spawn_command_line_async(command);
}

struct NoConnectionInfo : public WfNetworkConnectionInfo
{
    std::string get_icon_name(WfConnectionState state)
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

struct VPNConnectionInfo : public WfNetworkConnectionInfo
{
    VPNConnectionInfo(const std::shared_ptr<Gio::DBus::Connection>& connection, std::string path)
    { }
    virtual std::string get_icon_name(WfConnectionState state) override
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

    virtual ~VPNConnectionInfo()
    {}
};

struct WifiConnectionInfo : public WfNetworkConnectionInfo
{
    WayfireNetworkInfo *widget;
    DBusProxy ap;
    sigc::connection ap_sig;

    WifiConnectionInfo(const DBusConnection& connection, std::string path,
        WayfireNetworkInfo *widget)
    {
        this->widget = widget;

        ap = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME, path,
            "org.freedesktop.NetworkManager.AccessPoint");

        if (ap)
        {
            ap_sig = ap->signal_properties_changed().connect(
                sigc::mem_fun(*this, &WifiConnectionInfo::on_properties_changed));
        }
    }

    void on_properties_changed(DBusPropMap changed, DBusPropList invalid)
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
            widget->update_icon();
            widget->update_status();
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

    virtual std::string get_icon_name(WfConnectionState state)
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

    virtual ~WifiConnectionInfo()
    {}
};

struct EthernetConnectionInfo : public WfNetworkConnectionInfo
{
    DBusProxy ap;
    EthernetConnectionInfo(const DBusConnection& connection, std::string path)
    {}

    virtual std::string get_icon_name(WfConnectionState state)
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

    virtual ~EthernetConnectionInfo()
    {}
};


/* TODO: handle Connectivity */

static WfConnectionState get_connection_state(DBusProxy connection)
{
    if (!connection)
    {
        return CSTATE_DEACTIVATED;
    }

    Glib::Variant<guint32> state;
    connection->get_cached_property(state, "State");
    return (WfConnectionState)state.get();
}

void WayfireNetworkInfo::update_icon()
{
    auto icon_name = info->get_icon_name(
        get_connection_state(active_connection_proxy));
    icon.set_from_icon_name(icon_name);
}

struct status_color
{
    int point;
    Gdk::RGBA rgba;
} status_colors[] = {
    {0, Gdk::RGBA{"#ff0000"}},
    {25, Gdk::RGBA{"#ff0000"}},
    {40, Gdk::RGBA{"#ffff55"}},
    {100, Gdk::RGBA{"#00ff00"}},
};

#define MAX_COLORS (sizeof(status_colors) / sizeof(status_color))

void WayfireNetworkInfo::update_status()
{
    std::string description = info->get_connection_name();

    status.set_text(description);
    button.set_tooltip_text(description);

    status.remove_css_class("excellent");
    status.remove_css_class("good");
    status.remove_css_class("weak");
    status.remove_css_class("none");
    if (status_color_opt)
    {
        status.add_css_class(info->get_strength_str());
    }
}

void WayfireNetworkInfo::update_active_connection()
{
    Glib::Variant<Glib::ustring> active_conn_path;
    nm_proxy->get_cached_property(active_conn_path, ACTIVE_CONNECTION);

    if (active_conn_path && (active_conn_path.get() != "/"))
    {
        active_connection_proxy = Gio::DBus::Proxy::create_sync(
            connection, NM_DBUS_NAME, active_conn_path.get(),
            "org.freedesktop.NetworkManager.Connection.Active");
    } else
    {
        active_connection_proxy = DBusProxy();
    }

    auto set_no_connection = [=] ()
    {
        info = std::unique_ptr<WfNetworkConnectionInfo>(new NoConnectionInfo());
        info->connection_name = "No connection";
    };

    if (!active_connection_proxy)
    {
        set_no_connection();
    } else
    {
        Glib::Variant<Glib::ustring> vtype, vobject;
        active_connection_proxy->get_cached_property(vtype, "Type");
        active_connection_proxy->get_cached_property(vobject, "SpecificObject");
        auto type   = vtype.get();
        auto object = vobject.get();

        if (type.find("wireless") != type.npos)
        {
            info = std::unique_ptr<WfNetworkConnectionInfo>(
                new WifiConnectionInfo(connection, object, this));
        } else if (type.find("ethernet") != type.npos)
        {
            info = std::unique_ptr<WfNetworkConnectionInfo>(
                new EthernetConnectionInfo(connection, object));
        } else if (type.find("bluetooth") != type.npos)
        {
            std::cout << "Unimplemented: bluetooth connection" << std::endl;
            set_no_connection();
            // TODO
        } else if (type.find("vpn") != type.npos)
        {
            info = std::unique_ptr<WfNetworkConnectionInfo>(
                new VPNConnectionInfo(connection, object)
            );
        } else
        {
            std::cout << "Unimplemented: unknown connection type" << std::endl;
            set_no_connection();
            // TODO: implement Unknown connection
        }

        Glib::Variant<Glib::ustring> vname;
        active_connection_proxy->get_cached_property(vname, "Id");
        info->connection_name = vname.get();
    }

    update_icon();
    update_status();
}

void WayfireNetworkInfo::on_nm_properties_changed(
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

bool WayfireNetworkInfo::setup_dbus()
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
        sigc::mem_fun(*this, &WayfireNetworkInfo::on_nm_properties_changed)));

    return true;
}

void WayfireNetworkInfo::on_click()
{
    if ((std::string)click_command_opt != "default")
    {
        Glib::spawn_command_line_async((std::string)click_command_opt);
    } else
    {
        info->spawn_control_center(nm_proxy);
    }
}

void WayfireNetworkInfo::init(Gtk::Box *container)
{
    if (!setup_dbus())
    {
        enabled = false;
        return;
    }

    button.add_css_class("widget-icon");
    button.add_css_class("flat");
    button.add_css_class("network");

    container->append(button);
    button.set_child(button_content);
    button.add_css_class("flat");

    signals.push_back(button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireNetworkInfo::on_click)));

    button_content.set_valign(Gtk::Align::CENTER);
    button_content.append(icon);
    button_content.append(status);
    button_content.set_spacing(6);

    icon.set_valign(Gtk::Align::CENTER);
    signals.push_back(icon.property_scale_factor().signal_changed().connect(
        sigc::mem_fun(*this, &WayfireNetworkInfo::update_icon)));
    icon.add_css_class("network-icon");

    update_active_connection();
    handle_config_reload();
}

void WayfireNetworkInfo::handle_config_reload()
{
    if (status_opt.value() == NETWORK_STATUS_ICON)
    {
        if (status.get_parent())
        {
            button_content.remove(status);
        }
    } else
    {
        if (!status.get_parent())
        {
            button_content.append(status);
        }
    }

    // TODO: show IP for "full" status

    update_icon();
    update_status();
}

WayfireNetworkInfo::~WayfireNetworkInfo()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}
