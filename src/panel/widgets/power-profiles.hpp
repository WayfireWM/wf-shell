#pragma once

#include <gtkmm/image.h>

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>
#include <giomm/simpleactiongroup.h>

#include "widget.hpp"
#include "wf-popover.hpp"
#include "widget-utils/power-profiles.hpp"

class WayfirePowerProfiles : public WayfireWidget
{
    ShellPowerTools power_tools;
    Gtk::Image icon;
    std::unique_ptr<WayfireMenuWidget> panel_menu;

    void update_icon();
    void set_current_profile(Glib::ustring profile);

    void on_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

  public:
    virtual void init(Gtk::Box *container);
    virtual ~WayfirePowerProfiles() = default;
};
