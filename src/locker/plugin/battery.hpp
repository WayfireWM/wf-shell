#pragma once
#include <memory>
#include <string>
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#include <gtkmm/grid.h>
#include <giomm.h>

#include "plugin.hpp"
#include "lockergrid.hpp"

using DBusConnection = Glib::RefPtr<Gio::DBus::Connection>;
using DBusProxy = Glib::RefPtr<Gio::DBus::Proxy>;
/* Shamelessly copied in from battery in panel */

#define UPOWER_NAME "org.freedesktop.UPower"
#define DISPLAY_DEVICE "/org/freedesktop/UPower/devices/DisplayDevice"

#define ICON           "IconName"
#define TYPE           "Type"
#define STATE          "State"
#define PERCENTAGE     "Percentage"
#define TIMETOFULL     "TimeToFull"
#define TIMETOEMPTY    "TimeToEmpty"
#define SHOULD_DISPLAY "IsPresent"

class WayfireLockerBatteryPlugin : public WayfireLockerPlugin
{
  private:
    DBusConnection connection;
    DBusProxy upower_proxy, display_device;
    void on_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);
    bool setup_dbus();

  public:
    WayfireLockerBatteryPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;
    void hide();
    void show();
    bool show_state = true;

    void update_percentages(std::string text);
    void update_descriptions(std::string text);
    void update_images();
    void update_details();

    std::map<int, std::shared_ptr<Gtk::Image>> images;
    std::map<int, std::shared_ptr<Gtk::Label>> subtexts;
    std::map<int, std::shared_ptr<Gtk::Label>> labels;
    std::map<int, std::shared_ptr<Gtk::Grid>> grids;
};
