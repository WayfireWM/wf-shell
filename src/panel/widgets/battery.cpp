#include <gtk-utils.hpp>
#include <iostream>

#include "battery.hpp"
#define POWER_PROFILE_PATH "/org/freedesktop/UPower/PowerProfiles"
#define POWER_PROFILE_NAME "org.freedesktop.UPower.PowerProfiles"
#define UPOWER_NAME "org.freedesktop.UPower"
#define DISPLAY_DEVICE "/org/freedesktop/UPower/devices/DisplayDevice"

#define ICON           "IconName"
#define TYPE           "Type"
#define STATE          "State"
#define PERCENTAGE     "Percentage"
#define TIMETOFULL     "TimeToFull"
#define TIMETOEMPTY    "TimeToEmpty"
#define SHOULD_DISPLAY "IsPresent"

#define DEGRADED       "PerformanceDegraded"
#define PROFILES       "Profiles"
#define ACTIVE_PROFILE "ActiveProfile"

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

    box->set_tooltip_text(
        get_device_type_description(type.get()) + description);

    if (status_opt.value() == BATTERY_STATUS_PERCENT)
    {
        label.set_text(percentage_string);
        overlay.remove_overlay(label);
        box->append(label);
    } else if (status_opt.value() == BATTERY_STATUS_FULL)
    {
        label.set_text(description);
        auto children = overlay.get_children();
        if (std::count(children.begin(), children.end(), &label))
        {
            overlay.remove_overlay(label);
        }

        box->append(label);
    } else if (status_opt.value() == BATTERY_STATUS_OVERLAY)
    {
        label.set_text(percentage_string);
        auto children = box->get_children();
        if (std::count(children.begin(), children.end(), &label))
        {
            box->remove(label);
        }

        overlay.add_overlay(label);
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

    powerprofile_proxy = Gio::DBus::Proxy::create_sync(connection, POWER_PROFILE_NAME,
        POWER_PROFILE_PATH,
        POWER_PROFILE_NAME);
    if (!powerprofile_proxy)
    {
        std::cout << "Unable to conect to Power Profiles. Continuing" << std::endl;
    } else
    {
        powerprofile_proxy->signal_properties_changed().connect(
            sigc::mem_fun(*this, &WayfireBatteryInfo::on_upower_properties_changed));
        Glib::Variant<Glib::ustring> current_profile;
        Glib::Variant<std::vector<std::map<Glib::ustring, Glib::VariantBase>>> profiles;
        powerprofile_proxy->get_cached_property(current_profile, ACTIVE_PROFILE);
        powerprofile_proxy->get_cached_property(profiles, PROFILES);

        if (profiles && current_profile)
        {
            setup_profiles(profiles.get());
            set_current_profile(current_profile.get());
        } else
        {
            std::cout << "Unable to conect to Power Profiles. Continuing" << std::endl;
        }
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
        box->set_orientation(Gtk::Orientation::VERTICAL);
    } else
    {
        box->set_orientation(Gtk::Orientation::HORIZONTAL);
    }
}

void WayfireBatteryInfo::handle_config_reload()
{
    update_layout();
}

// TODO: simplify config loading

void WayfireBatteryInfo::init(Gtk::Box *container)
{
    profiles_menu = Gio::Menu::create();
    state_action  = Gio::SimpleAction::create_radio_string("set_profile", "");
    box = std::make_unique<WayfireMenuWidget>("panel", "battery");

    if (!setup_dbus())
    {
        return;
    }

    box->append(overlay);
    overlay.set_child(icon);
    icon.add_css_class("widget-icon");

    status_opt.set_callback([=] () { update_details(); });

    update_details();
    update_icon();
    auto actions = Gio::SimpleActionGroup::create();

    state_action->signal_activate().connect([=] (Glib::VariantBase vb)
    {
        // User has requested a change of state. Don't change the UI choice, let the dbus roundtrip happen to
        // be sure.
        if (vb.is_of_type(Glib::VariantType("s")))
        {
            // Couldn't seem to make proxy send property back, so this will have to do
            Glib::VariantContainerBase params = Glib::Variant<std::tuple<Glib::ustring, Glib::ustring,
                Glib::VariantBase>>::create({POWER_PROFILE_NAME, ACTIVE_PROFILE, vb});

            connection->call_sync(
                POWER_PROFILE_PATH,
                "org.freedesktop.DBus.Properties",
                "Set",
                params,
                NULL,
                POWER_PROFILE_NAME,
                -1,
                Gio::DBus::CallFlags::NONE,
                {});
        }
    });

    box->open_on(1);

    actions->add_action(state_action);
    box->insert_action_group("actions", actions);
    container->append(*box);
    box->set_spacing(5);
    box->set_menu_model(profiles_menu);

    icon.property_scale_factor().signal_changed()
        .connect(sigc::mem_fun(*this, &WayfireBatteryInfo::update_icon));

    update_layout();
}

void WayfireBatteryInfo::on_upower_properties_changed(
    const Gio::DBus::Proxy::MapChangedProperties& properties,
    const std::vector<Glib::ustring>& invalidated)
{
    for (auto& prop : properties)
    {
        if (prop.first == ACTIVE_PROFILE)
        {
            if (prop.second.is_of_type(Glib::VariantType("s")))
            {
                auto value_string =
                    Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(prop.second).get();
                set_current_profile(value_string);
            }
        } else if (prop.first == PROFILES)
        {
            // I've been unable to find a way to change possible profiles on the fly, so cannot confirm this
            // works at all.
            auto value = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<std::map<Glib::ustring,
                Glib::VariantBase>>>>(prop.second);
            setup_profiles(value.get());
        }

        // TODO Consider watching for "Performance Degraded" events too, but we currently have no way to
        // output this additional information
    }
}

void WayfireBatteryInfo::set_current_profile(Glib::ustring profile)
{
    state_action->set_state(Glib::Variant<Glib::ustring>::create(profile));
}

void WayfireBatteryInfo::setup_profiles(std::vector<std::map<Glib::ustring, Glib::VariantBase>> profiles)
{
    profiles_menu->remove_all();
    for (auto profile : profiles)
    {
        if (profile.count("Profile") == 1)
        {
            Glib::VariantBase value = profile.at("Profile");
            if (value.is_of_type(Glib::VariantType("s")))
            {
                auto value_string =
                    Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(value).get();
                auto item = Gio::MenuItem::create(value_string, "noactionyet");

                item->set_action_and_target("actions.set_profile",
                    Glib::Variant<Glib::ustring>::create(value_string));
                profiles_menu->append_item(item);
            }
        }
    }
}

WayfireBatteryInfo::~WayfireBatteryInfo()
{
    btn_sig.disconnect();
    disp_dev_sig.disconnect();
}
