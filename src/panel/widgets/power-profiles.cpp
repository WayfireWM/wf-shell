#include <iostream>
#include <tuple>

#include "power-profiles.hpp"

#define POWER_PROFILE_PATH "/org/freedesktop/UPower/PowerProfiles"
#define POWER_PROFILE_NAME "org.freedesktop.UPower.PowerProfiles"

#define PROFILES       "Profiles"
#define ACTIVE_PROFILE "ActiveProfile"

void WayfirePowerProfiles::update_icon()
{
    icon.set_from_icon_name("power-profile-" + power_mode);
}

void WayfirePowerProfiles::set_current_profile(Glib::ustring profile)
{
    power_mode = profile;
    state_action->set_state(Glib::Variant<Glib::ustring>::create(profile));
    update_icon();
}

void WayfirePowerProfiles::setup_profiles(std::vector<std::map<Glib::ustring, Glib::VariantBase>> profiles)
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

void WayfirePowerProfiles::on_properties_changed(
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
            auto value = Glib::VariantBase::cast_dynamic<Glib::Variant<std::vector<std::map<Glib::ustring,
                Glib::VariantBase>>>>(prop.second);
            setup_profiles(value.get());
        }
    }
}

bool WayfirePowerProfiles::setup_dbus_power_modes()
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
        return false;
    }

    powerprofile_proxy->signal_properties_changed().connect(
        sigc::mem_fun(*this, &WayfirePowerProfiles::on_properties_changed));

    Glib::Variant<Glib::ustring> current_profile;
    Glib::Variant<std::vector<std::map<Glib::ustring, Glib::VariantBase>>> profiles;
    powerprofile_proxy->get_cached_property(current_profile, ACTIVE_PROFILE);
    powerprofile_proxy->get_cached_property(profiles, PROFILES);

    if (profiles && current_profile)
    {
        setup_profiles(profiles.get());
        set_current_profile(current_profile.get());
        return true;
    }

    std::cout << "Unable to conect to Power Profiles. Continuing" << std::endl;
    return false;
}

void WayfirePowerProfiles::init(Gtk::Box *container)
{
    profiles_menu = Gio::Menu::create();
    state_action  = Gio::SimpleAction::create_radio_string("set_profile", "");

    if (!setup_dbus_power_modes())
    {
        return;
    }

    box = std::make_unique<WayfireMenuWidget>("panel", "power-profiles");

    box->set_child(icon);
    icon.add_css_class("widget-icon");

    auto actions = Gio::SimpleActionGroup::create();

    state_action->signal_activate().connect([=] (Glib::VariantBase vb)
    {
        if (vb.is_of_type(Glib::VariantType("s")))
        {
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

    actions->add_action(state_action);

    box->open_on(1);
    box->insert_action_group("actions", actions);
    box->set_spacing(5);
    box->set_menu_model(profiles_menu);

    container->append(*box);

    icon.property_scale_factor().signal_changed()
        .connect(sigc::mem_fun(*this, &WayfirePowerProfiles::update_icon));

    update_icon();
}
