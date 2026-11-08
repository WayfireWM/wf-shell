#pragma once

#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/overlay.h>

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>

#include <sigc++/connection.h>

#include <wf-option-wrap.hpp>

using DBusConnection = Glib::RefPtr<Gio::DBus::Connection>;
using DBusProxy = Glib::RefPtr<Gio::DBus::Proxy>;

static const std::string BATTERY_STATUS_ICON    = "icon"; // icon
static const std::string BATTERY_STATUS_PERCENT = "percentage"; // icon + percentage
static const std::string BATTERY_STATUS_FULL    = "full"; // icon + percentage + TimeToFull/TimeToEmpty
static const std::string BATTERY_STATUS_OVERLAY = "percentage_overlay";

class ShellBattery : public Gtk::Box
{
    WfOption<std::string> status_opt;

    sigc::connection disp_dev_sig;

    Gtk::Label label;
    Gtk::Overlay overlay;
    Gtk::Image icon;

    DBusConnection connection;
    DBusProxy upower_proxy, display_device;

    bool upower_avail;
    bool setup();

    void update_icon();
    void update_details();
    void update_state();

    void on_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

  public:
    ShellBattery(const std::string& section);

    bool get_upower_avail();

    ~ShellBattery();
};
