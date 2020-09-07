#include "network.hpp"
#include <glibmm/spawn.h>
#include <cassert>
#include <iostream>
#include <gtk-utils.hpp>

#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define ACTIVE_CONNECTION "PrimaryConnection"
#define STRENGTH "Strength"

// status options
#define STATUS_CONN_ICON "icon_only"
#define STATUS_CONN_NAME "connection_name"

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

    std::string get_ip()
    {
        return "127.0.0.1";
    }

    virtual ~NoConnectionInfo() {}
};

struct WifiConnectionInfo : public WfNetworkConnectionInfo
{
    WayfireNetworkInfo *widget;
    DBusProxy ap;

    WifiConnectionInfo(const DBusConnection& connection, std::string path,
                       WayfireNetworkInfo *widget)
    {
        this->widget = widget;

        ap = Gio::DBus::Proxy::create_sync(connection, NM_DBUS_NAME, path,
                                           "org.freedesktop.NetworkManager.AccessPoint");

        if (ap) {
            ap->signal_properties_changed().connect_notify(
                sigc::mem_fun(this, &WifiConnectionInfo::on_properties_changed));
        }
    }

    void on_properties_changed(DBusPropMap changed, DBusPropList invalid)
    {
        bool needs_refresh = false;
        for (auto& prop : changed)
        {
            if (prop.first == STRENGTH)
                needs_refresh = true;
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
            return "excellent";
        if (value > 55)
            return "good";
        if (value > 30)
            return "ok";
        if (value > 5)
            return "weak";
        return "none";
    }

    virtual std::string get_icon_name(WfConnectionState state)
    {
        if (state <= CSTATE_ACTIVATING || state == CSTATE_DEACTIVATING)
            return "network-wireless-acquiring-symbolic";

        if (state == CSTATE_DEACTIVATED)
            return "network-wireless-disconnected-symbolic";

        if (ap) {
            return "network-wireless-signal-" + get_strength_str() + "-symbolic";
        } else {
            return "network-wireless-no-route-symbolic";
        }
    }

    virtual int get_connection_strength()
    {
        if (ap) {
            return get_strength();
        } else {
            return 100;
        }
    }

    virtual std::string get_ip()
    {
        return "0.0.0.0";
    }

    virtual ~WifiConnectionInfo() {}
};

struct EthernetConnectionInfo : public WfNetworkConnectionInfo
{
    DBusProxy ap;
    EthernetConnectionInfo(const DBusConnection& connection, std::string path)
    { }

    virtual std::string get_icon_name(WfConnectionState state)
    {
        if (state <= CSTATE_ACTIVATING || state == CSTATE_DEACTIVATING)
            return "network-wired-acquiring-symbolic";

        if (state == CSTATE_DEACTIVATED)
            return "network-wired-disconnected-symbolic";

        return "network-wired-symbolic";
    }

    std::string get_connection_name()
    {
        return "Ethernet - " + connection_name;
    }

    virtual int get_connection_strength()
    {
        return 100;
    }

    virtual std::string get_ip()
    {
        return "0.0.0.0";
    }

    virtual ~EthernetConnectionInfo() {}
};


/* TODO: handle Connectivity */

static WfConnectionState get_connection_state(DBusProxy connection)
{
    if (!connection)
        return CSTATE_DEACTIVATED;

    Glib::Variant<guint32> state;
    connection->get_cached_property(state, "State");
    return (WfConnectionState)state.get();
}

void WayfireNetworkInfo::update_icon()
{
    auto icon_name = info->get_icon_name(
        get_connection_state(active_connection_proxy));
    WfIconLoadOptions options;
    options.invert = icon_invert_opt;
    options.user_scale = icon.get_scale_factor();
    set_image_icon(icon, icon_name, icon_size_opt, options);
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

static Gdk::RGBA get_color_for_pc(int pc)
{
    for (int i = MAX_COLORS - 2; i >= 0; i--)
    {
        if (status_colors[i].point <= pc)
        {
            auto& r1 = status_colors[i].rgba;
            auto& r2 = status_colors[i + 1].rgba;

            double a = 1.0 * (pc - status_colors[i].point) / (status_colors[i + 1].point - status_colors[i].point);
            Gdk::RGBA result;
            result.set_rgba(
                r1.get_red  () * (1 - a) + r2.get_red  () * a,
                r1.get_green() * (1 - a) + r2.get_green() * a,
                r1.get_blue () * (1 - a) + r2.get_blue () * a,
                r1.get_alpha() * (1 - a) + r2.get_alpha() * a);

            return result;
        }
    }

    return Gdk::RGBA{"#ffffff"};
}

void WayfireNetworkInfo::update_status()
{
    std::string description = info->get_connection_name();

    status.set_text(description);
    button.set_tooltip_text(description);

    if (status_color_opt) {
        status.override_color(get_color_for_pc(info->get_connection_strength()));
    } else {
        status.unset_color();
    }
}

void WayfireNetworkInfo::update_active_connection()
{
    Glib::Variant<Glib::ustring> active_conn_path;
    nm_proxy->get_cached_property(active_conn_path, ACTIVE_CONNECTION);

    if (active_conn_path && active_conn_path.get() != "/")
    {
        active_connection_proxy = Gio::DBus::Proxy::create_sync(
            connection, NM_DBUS_NAME, active_conn_path.get(),
            "org.freedesktop.NetworkManager.Connection.Active");
    } else {
        active_connection_proxy = DBusProxy();
    }

    auto set_no_connection = [=] ()
    {
        info = std::unique_ptr<WfNetworkConnectionInfo> (new NoConnectionInfo());
        info->connection_name = "No connection";
    };

    if (!active_connection_proxy)
    {
        set_no_connection();
    } else {
        Glib::Variant<Glib::ustring> vtype, vobject;
        active_connection_proxy->get_cached_property(vtype, "Type");
        active_connection_proxy->get_cached_property(vobject, "SpecificObject");
        auto type = vtype.get();
        auto object = vobject.get();

        if (type.find("wireless") != type.npos)
        {
            info = std::unique_ptr<WfNetworkConnectionInfo> (
                new WifiConnectionInfo(connection, object, this));
        }
        else if (type.find("ethernet") != type.npos)
        {
            info = std::unique_ptr<WfNetworkConnectionInfo> (
                new EthernetConnectionInfo(connection, object));
        } else if (type.find("bluetooth"))
        {
            std::cout << "Unimplemented: bluetooth connection" << std::endl;
            set_no_connection();
            // TODO
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
    for (auto &prop : properties)
    {
        if (prop.first == ACTIVE_CONNECTION)
            update_active_connection();
    }
}

bool WayfireNetworkInfo::setup_dbus()
{
    auto cancellable = Gio::Cancellable::create();
    connection = Gio::DBus::Connection::get_sync(Gio::DBus::BUS_TYPE_SYSTEM, cancellable);
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
        std::cerr << "Failed to connect to network manager, "
            << "are you sure it is running?" << std::endl;
        return false;
    }

    nm_proxy->signal_properties_changed().connect_notify(
        sigc::mem_fun(this, &WayfireNetworkInfo::on_nm_properties_changed));

    return true;
}

void WayfireNetworkInfo::on_click()
{
    info->spawn_control_center(nm_proxy);
}

void WayfireNetworkInfo::init(Gtk::HBox *container)
{
    if (!setup_dbus())
    {
        enabled = false;
        return;
    }

    container->add(button);
    button.add(button_content);
    button.get_style_context()->add_class("flat");

    button.signal_clicked().connect_notify(
        sigc::mem_fun(this, &WayfireNetworkInfo::on_click));

    button_content.set_valign(Gtk::ALIGN_CENTER);
    button_content.pack_start(icon, Gtk::PACK_SHRINK);
    button_content.pack_start(status, Gtk::PACK_SHRINK);
    button_content.set_spacing(6);

    icon.set_valign(Gtk::ALIGN_CENTER);
    icon.property_scale_factor().signal_changed().connect(
        sigc::mem_fun(this, &WayfireNetworkInfo::update_icon));

    update_active_connection();
    handle_config_reload();
}

void WayfireNetworkInfo::handle_config_reload()
{
    if ((std::string)status_font_opt == "default") {
        status.unset_font();
    } else {
        status.override_font(
            Pango::FontDescription((std::string)status_font_opt));
    }

    if (status_opt < NETWORK_STATUS_CONN_NAME)
    {
        if (status.get_parent())
            button_content.remove(status);
    } else
    {
        if (!status.get_parent())
        {
            button_content.pack_start(status);
            button_content.show_all();
        }
    }

    update_icon();
    update_status();
}

WayfireNetworkInfo::~WayfireNetworkInfo()
{
}
