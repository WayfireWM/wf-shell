#pragma once
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <memory>
#include "giomm/dbusconnection.h"
#include "giomm/dbusproxy.h"
#include "lockergrid.hpp"
#include "plugin.hpp"


enum WfPluginConnectionState // NmActiveConnectionState
{
    CSTATE_UNKNOWN      = 0,
    CSTATE_ACTIVATING   = 1,
    CSTATE_ACTIVATED    = 2,
    CSTATE_DEACTIVATING = 3,
    CSTATE_DEACTIVATED  = 4,
};

struct WfPluginNetworkConnectionInfo
{
    std::string connection_name;

    virtual std::string get_connection_name()
    {
        return connection_name;
    }

    virtual std::string get_icon_name(WfPluginConnectionState state) = 0;
    virtual int get_connection_strength() = 0;
    virtual std::string get_ip() = 0;
    virtual std::string get_strength_str() = 0;

    virtual ~WfPluginNetworkConnectionInfo()
    {}
};

class WayfireLockerNetworkPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    WayfireLockerNetworkPluginWidget(std::string image_contents, std::string label_contents, std::string css_contents);
    Gtk::Label label;
    Gtk::Image image;
    Gtk::Box box;
};

class WayfireLockerNetworkPlugin : public WayfireLockerPlugin{
  private:
    std::shared_ptr<Gio::DBus::Connection> connection;
    std::shared_ptr<Gio::DBus::Proxy> nm_proxy, active_connection_proxy;
    std::vector<sigc::connection> signals;
    std::unique_ptr<WfPluginNetworkConnectionInfo> info;

  public:
    WayfireLockerNetworkPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;

    bool setup_dbus();
    void on_nm_properties_changed(const Gio::DBus::Proxy::MapChangedProperties& properties, const std::vector<Glib::ustring>& invalidated);
    void update_active_connection();
    void set_state();
    std::unordered_map<int, std::shared_ptr<WayfireLockerNetworkPluginWidget>> widgets;

    std::string image_contents = "network-error-symbolic", label_contents = "Unknown state", css_contents="";
};