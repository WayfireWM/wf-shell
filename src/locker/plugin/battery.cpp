#include <memory>
#include <iostream>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include "lockergrid.hpp"
#include "battery.hpp"


static const std::string BATTERY_STATUS_ICON    = "icon"; // icon
static const std::string BATTERY_STATUS_PERCENT = "percentage"; // icon + percentage
static const std::string BATTERY_STATUS_FULL    = "full"; // icon + percentage + TimeToFull/TimeToEmpty

static bool is_charging(uint32_t state)
{
    return (state == 1) || (state == 5);
}

static bool is_discharging(uint32_t state)
{
    return (state == 2) || (state == 6);
}

static std::string state_descriptions[] = {
    "Unknown", // 0
    "Charging", // 1
    "Discharging", // 2
    "Empty", // 3
    "Fully charged", // 4
    "Pending charge", // 5
    "Pending discharge", // 6
};

static std::string get_device_type_description(uint32_t type)
{
    if (type == 2)
    {
        return "Battery ";
    }

    if (type == 3)
    {
        return "UPS ";
    }

    return "";
}

static std::string format_digit(int digit)
{
    return digit <= 9 ? ("0" + std::to_string(digit)) :
           std::to_string(digit);
}

static std::string uint_to_time(int64_t time)
{
    int hrs = time / 3600;
    int min = (time / 60) % 60;

    return format_digit(hrs) + ":" + format_digit(min);
}

bool WayfireLockerBatteryPlugin::should_enable()
{
    return (bool)enable;
}

void WayfireLockerBatteryPlugin::update_percentages(std::string text)
{
    for (auto& it : labels)
    {
        it.second->set_label(text);
    }
}

void WayfireLockerBatteryPlugin::update_descriptions(std::string text)
{
    for (auto& it : subtexts)
    {
        it.second->set_label(text);
    }
}

void WayfireLockerBatteryPlugin::update_images()
{
    Glib::Variant<Glib::ustring> icon_name;
    display_device->get_cached_property(icon_name, ICON);
    for (auto& it : images)
    {
        it.second->set_from_icon_name(icon_name.get());
    }
}

void WayfireLockerBatteryPlugin::add_output(int id, WayfireLockerGrid *grid)
{
    Gtk::Grid *batt_grid = new Gtk::Grid();
    labels.emplace(id, std::shared_ptr<Gtk::Label>(new Gtk::Label()));
    subtexts.emplace(id, std::shared_ptr<Gtk::Label>(new Gtk::Label()));
    images.emplace(id, std::shared_ptr<Gtk::Image>(new Gtk::Image()));
    auto label   = labels[id];
    auto subtext = subtexts[id];
    auto image   = images[id];

    if (!show_state)
    {
        label->hide();
        subtext->hide();
        image->hide();
    }

    label->add_css_class("battery-percent");
    subtext->add_css_class("battery-description");
    image->add_css_class("battery-image");

    batt_grid->attach(*image, 0, 0);
    batt_grid->attach(*label, 1, 0);
    batt_grid->attach(*subtext, 0, 1, 2, 1);

    grid->attach(*batt_grid, (std::string)battery_position);

    update_details();
}

void WayfireLockerBatteryPlugin::remove_output(int id)
{
    labels.erase(id);
}

void WayfireLockerBatteryPlugin::init()
{
    if (!setup_dbus())
    {
        return;
    }
}

bool WayfireLockerBatteryPlugin::setup_dbus()
{
    auto cancellable = Gio::Cancellable::create();
    connection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SYSTEM, cancellable);
    if (!connection)
    {
        std::cerr << "Failed to connect to dbus" << std::endl;
        return false;
    }

    upower_proxy = Gio::DBus::Proxy::create_sync(connection, UPOWER_NAME,
        "/org/freedesktop/UPower",
        "org.freedesktop.UPower");
    if (!upower_proxy)
    {
        std::cerr << "Failed to connect to UPower" << std::endl;
        return false;
    }

    display_device = Gio::DBus::Proxy::create_sync(connection,
        UPOWER_NAME,
        DISPLAY_DEVICE,
        "org.freedesktop.UPower.Device");
    if (!display_device)
    {
        return false;
    }

    Glib::Variant<bool> present;
    display_device->get_cached_property(present, SHOULD_DISPLAY);
    if (present.get())
    {
        display_device->signal_properties_changed().connect(
            sigc::mem_fun(*this, &WayfireLockerBatteryPlugin::on_properties_changed));

        return true;
    }

    return false;
}

void WayfireLockerBatteryPlugin::on_properties_changed(
    const Gio::DBus::Proxy::MapChangedProperties& properties,
    const std::vector<Glib::ustring>& invalidated)
{
    bool invalid_icon = false, invalid_details = false;
    for (auto& prop : properties)
    {
        if (prop.first == ICON)
        {
            invalid_icon = true;
        }

        if ((prop.first == TYPE) || (prop.first == STATE) || (prop.first == PERCENTAGE) ||
            (prop.first == TIMETOFULL) || (prop.first == TIMETOEMPTY))
        {
            invalid_details = true;
        }

        if (prop.first == SHOULD_DISPLAY)
        {}
    }

    if (invalid_icon)
    {
        update_images();
    }

    if (invalid_details)
    {
        update_details();
    }
}

void WayfireLockerBatteryPlugin::update_details()
{
    Glib::Variant<guint32> type;
    display_device->get_cached_property(type, TYPE);

    Glib::Variant<guint32> vstate;
    display_device->get_cached_property(vstate, STATE);
    uint32_t state = vstate.get();

    Glib::Variant<gdouble> vpercentage;
    display_device->get_cached_property(vpercentage, PERCENTAGE);
    auto percentage_string = std::to_string((int)vpercentage.get()) + "%";

    Glib::Variant<gint64> time_to_full;
    display_device->get_cached_property(time_to_full, TIMETOFULL);

    Glib::Variant<gint64> time_to_empty;
    display_device->get_cached_property(time_to_empty, TIMETOEMPTY);

    std::string description = state_descriptions[state];
    if (is_charging(state))
    {
        description += ", " + uint_to_time(time_to_full.get()) + " until full";
    } else if (is_discharging(state))
    {
        description += ", " + uint_to_time(time_to_empty.get()) + " remaining";
    }

    if (state == 0) /* Unknown */
    {
        hide();
        return;
    }

    show();
    update_descriptions(get_device_type_description(type.get()) + description);
    update_percentages(percentage_string);
    update_images();
}

void WayfireLockerBatteryPlugin::hide()
{
    show_state = false;
    for (auto& it : labels)
    {
        it.second->hide();
    }

    for (auto& it : images)
    {
        it.second->hide();
    }

    for (auto & it : subtexts)
    {
        it.second->hide();
    }
}

void WayfireLockerBatteryPlugin::show()
{
    show_state = true;
    for (auto& it : labels)
    {
        it.second->show();
    }

    for (auto& it : images)
    {
        it.second->show();
    }

    for (auto& it : subtexts)
    {
        it.second->show();
    }
}
