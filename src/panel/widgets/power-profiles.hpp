#pragma once

#include <gtkmm/image.h>

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>
#include <giomm/simpleactiongroup.h>

#include "widget.hpp"
#include "wf-popover.hpp"

using DBusConnection = Glib::RefPtr<Gio::DBus::Connection>;
using DBusProxy = Glib::RefPtr<Gio::DBus::Proxy>;

class WayfirePowerProfiles : public WayfireWidget
{
    Gtk::Image icon;
    std::unique_ptr<WayfireMenuWidget> box;
    std::shared_ptr<Gio::Menu> profiles_menu;
    std::shared_ptr<Gio::SimpleAction> state_action;

    DBusConnection connection;
    DBusProxy powerprofile_proxy;
    std::string power_mode = "";

    bool setup_dbus_power_modes();

    void update_icon();
    void set_current_profile(Glib::ustring profile);
    void setup_profiles(std::vector<std::map<Glib::ustring, Glib::VariantBase>> profiles);

    void on_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

  public:
    virtual void init(Gtk::Box *container);
    virtual ~WayfirePowerProfiles() = default;
};
