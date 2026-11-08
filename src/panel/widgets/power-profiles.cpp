#include <iostream>
#include <tuple>

#include "power-profiles.hpp"

void WayfirePowerProfiles::update_icon()
{
    icon.set_from_icon_name("power-profile-" + power_tools.power_mode);
}

void WayfirePowerProfiles::set_current_profile(Glib::ustring profile)
{
    power_tools.power_mode = profile;
    power_tools.state_action->set_state(Glib::Variant<Glib::ustring>::create(profile));
    update_icon();
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
            power_tools.setup_profiles(value.get());
        }
    }
}

void WayfirePowerProfiles::init(Gtk::Box *container)
{
    panel_menu = std::make_unique<WayfireMenuWidget>("panel", "power-profiles");

    panel_menu->set_child(icon);
    icon.add_css_class("widget-icon");

    panel_menu->open_on(1);
    panel_menu->insert_action_group("actions", power_tools.actions);
    panel_menu->set_spacing(5);
    panel_menu->set_menu_model(power_tools.profiles_menu);

    power_tools.powerprofile_proxy->signal_properties_changed().connect(
        sigc::mem_fun(*this, &WayfirePowerProfiles::on_properties_changed));

    container->append(*panel_menu);

    icon.property_scale_factor().signal_changed()
        .connect(sigc::mem_fun(*this, &WayfirePowerProfiles::update_icon));

    update_icon();
}
