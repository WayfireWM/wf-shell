#pragma once

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <sigc++/connection.h>

#include "../widget.hpp"

using DBusConnection = Glib::RefPtr<Gio::DBus::Connection>;
using DBusProxy = Glib::RefPtr<Gio::DBus::Proxy>;

using DBusPropMap  = const Gio::DBus::Proxy::MapChangedProperties&;
using DBusPropList = const std::vector<Glib::ustring>&;

enum WfConnectionState // NmActiveConnectionState
{
    CSTATE_UNKNOWN      = 0,
    CSTATE_ACTIVATING   = 1,
    CSTATE_ACTIVATED    = 2,
    CSTATE_DEACTIVATING = 3,
    CSTATE_DEACTIVATED  = 4,
};

struct WfNetworkConnectionInfo
{
    std::string connection_name;

    virtual void spawn_control_center(DBusProxy& nm);
    virtual std::string get_control_center_section(DBusProxy& nm);

    virtual std::string get_connection_name()
    {
        return connection_name;
    }

    virtual std::string get_icon_name(WfConnectionState state) = 0;
    virtual int get_connection_strength() = 0;
    virtual std::string get_ip() = 0;
    virtual std::string get_strength_str() = 0;

    virtual ~WfNetworkConnectionInfo()
    {}
};

static const std::string NETWORK_STATUS_ICON = "none";
static const std::string NETWORK_STATUS_CONN_NAME = "connection";
static const std::string NETWORK_STATUS_NAME_IP   = "full";

class WayfireNetworkInfo : public WayfireWidget
{
    DBusConnection connection;
    DBusProxy nm_proxy, active_connection_proxy;

    std::unique_ptr<WfNetworkConnectionInfo> info;

    std::vector<sigc::connection> signals;

    Gtk::Button button;
    Gtk::Box button_content;
    Gtk::Image icon;
    Gtk::Label status;

    bool enabled = true;
    WfOption<std::string> status_opt{"panel/network_status"};
    WfOption<bool> status_color_opt{"panel/network_status_use_color"};
    WfOption<std::string> status_font_opt{"panel/network_status_font"};
    WfOption<std::string> click_command_opt{"panel/network_onclick_command"};

    bool setup_dbus();
    void update_active_connection();
    void on_nm_properties_changed(DBusPropMap properties,
        DBusPropList invalidated);

    void on_click();

  public:
    void update_icon();
    void update_status();

    void init(Gtk::Box *container);

    void update_layout();
    void handle_config_reload();
    virtual ~WayfireNetworkInfo();
};
