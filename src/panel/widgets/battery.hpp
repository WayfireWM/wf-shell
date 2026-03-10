#pragma once

#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/overlay.h>

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>

#include <sigc++/connection.h>

#include "../widget.hpp"

using DBusConnection = Glib::RefPtr<Gio::DBus::Connection>;
using DBusProxy = Glib::RefPtr<Gio::DBus::Proxy>;

static const std::string BATTERY_STATUS_ICON    = "icon"; // icon
static const std::string BATTERY_STATUS_PERCENT = "percentage"; // icon + percentage
static const std::string BATTERY_STATUS_FULL    = "full"; // icon + percentage + TimeToFull/TimeToEmpty
static const std::string BATTERY_STATUS_OVERLAY = "percentage_overlay";

class wayfire_config;
class WayfireBatteryInfo : public WayfireWidget
{
    WfOption<std::string> status_opt{"panel/battery_status"};

    sigc::connection btn_sig, disp_dev_sig;

    Gtk::Button button;
    Gtk::Label label;
    Gtk::Box button_box;
    Gtk::Overlay overlay;

    Gtk::Image icon;

    DBusConnection connection;
    DBusProxy upower_proxy, display_device;

    bool setup_dbus();

    void update_icon();
    void update_details();
    void update_state();

    void on_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

  public:
    virtual void init(Gtk::Box *container);
    virtual ~WayfireBatteryInfo();
};
