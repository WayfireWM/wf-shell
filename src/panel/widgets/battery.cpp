#include <gtk-utils.hpp>
#include <iostream>

#include "battery.hpp"

#define UPOWER_NAME "org.freedesktop.UPower"
#define DISPLAY_DEVICE "/org/freedesktop/UPower/devices/DisplayDevice"

#define ICON           "IconName"
#define TYPE           "Type"
#define STATE          "State"
#define PERCENTAGE     "Percentage"
#define TIMETOFULL     "TimeToFull"
#define TIMETOEMPTY    "TimeToEmpty"
#define SHOULD_DISPLAY "IsPresent"

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

void WayfireBatteryInfo::on_properties_changed(
    const Gio::DBus::Proxy::MapChangedProperties& properties,
    const std::vector<Glib::ustring>& invalidated)
{
    bool invalid_icon = false, invalid_details = false;
    bool invalid_state = false;
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
        {
            invalid_state = true;
        }
    }

    if (invalid_icon)
    {
        update_icon();
    }

    if (invalid_details)
    {
        update_details();
    }

    if (invalid_state)
    {
        update_state();
    }
}

void WayfireBatteryInfo::update_icon()
{
    Glib::Variant<Glib::ustring> icon_name;
    display_device->get_cached_property(icon_name, ICON);
    icon.set_from_icon_name(icon_name.get());
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

static bool is_charging(uint32_t state)
{
    return (state == 1) || (state == 5);
}

static bool is_discharging(uint32_t state)
{
    return (state == 2) || (state == 6);
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

void WayfireBatteryInfo::update_details()
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

    std::string description = percentage_string + ", " + state_descriptions[state];
    if (is_charging(state))
    {
        description += ", " + uint_to_time(time_to_full.get()) + " until full";
    } else if (is_discharging(state))
    {
        description += ", " + uint_to_time(time_to_empty.get()) + " remaining";
    }

    button.set_tooltip_text(
        get_device_type_description(type.get()) + description);

    if (status_opt.value() == BATTERY_STATUS_PERCENT)
    {
        label.set_text(percentage_string);
    } else if (status_opt.value() == BATTERY_STATUS_FULL)
    {
        label.set_text(description);
    }

    if (status_opt.value() == BATTERY_STATUS_ICON)
    {
        label.hide();
    } else
    {
        label.show();
    }
}

void WayfireBatteryInfo::update_state()
{
    std::cout << "unimplemented reached, in battery.cpp: "
                 "\n\tWayfireBatteryInfo::update_state()" << std::endl;
}

bool WayfireBatteryInfo::setup_dbus()
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
        disp_dev_sig = display_device->signal_properties_changed().connect(
            sigc::mem_fun(*this, &WayfireBatteryInfo::on_properties_changed));

        return true;
    }

    return false;
}

void WayfireBatteryInfo::update_layout()
{
    WfOption<std::string> panel_position{"panel/position"};

    if (panel_position.value() == PANEL_POSITION_LEFT or panel_position.value() == PANEL_POSITION_RIGHT)
    {
        button_box.set_orientation(Gtk::Orientation::VERTICAL);
    } else
    {
        button_box.set_orientation(Gtk::Orientation::HORIZONTAL);
    }
}

void WayfireBatteryInfo::handle_config_reload()
{
    update_layout();
}

// TODO: simplify config loading

void WayfireBatteryInfo::init(Gtk::Box *container)
{
    if (!setup_dbus())
    {
        return;
    }

    button_box.append(icon);
    icon.add_css_class("widget-icon");
    button.add_css_class("battery");
    button.add_css_class("flat");

    status_opt.set_callback([=] () { update_details(); });

    update_details();
    update_icon();

    container->append(button);
    button_box.append(label);
    button_box.set_spacing(5);

    button.set_child(button_box);
    button.property_scale_factor().signal_changed()
        .connect(sigc::mem_fun(*this, &WayfireBatteryInfo::update_icon));

    update_layout();
}

WayfireBatteryInfo::~WayfireBatteryInfo()
{
    btn_sig.disconnect();
    disp_dev_sig.disconnect();
}
