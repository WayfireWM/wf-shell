#pragma once

#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/overlay.h>

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>
#include <giomm/simpleactiongroup.h>

#include <sigc++/connection.h>

#include "widget.hpp"
#include "wf-popover.hpp"

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

    Gtk::Label label;
    std::unique_ptr<WayfireMenuWidget> box;
    Gtk::Overlay overlay;
    std::shared_ptr<Gio::Menu> profiles_menu;
    Gtk::Image icon;

    DBusConnection connection;
    DBusProxy upower_proxy, powerprofile_proxy, display_device;

    bool setup_dbus_power_modes();
    bool setup_dbus_battery();

    void update_icon();
    void update_details();
    void update_state();

    void update_layout();
    void handle_config_reload();

    void on_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

    void on_upower_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

    void set_current_profile(Glib::ustring profile);
    void setup_profiles(std::vector<std::map<Glib::ustring, Glib::VariantBase>> profiles);

    std::shared_ptr<Gio::SimpleAction> state_action;

  public:
    virtual void init(Gtk::Box *container);
    virtual ~WayfireBatteryInfo();
};
