#include "launchers.hpp"
#include <giomm/file.h>
#include <glibmm/spawn.h>
#include <glibmm/keyfile.h>
#include <gtkmm/icontheme.h>
#include <gdk/gdkcairo.h>
#include <cassert>
#include <iostream>
#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>

bool WfLauncherButton::initialize(std::string name, std::string icon, std::string label)
{
    app_info = Gio::DesktopAppInfo::create(name);

    if (icon == "none")
    {
        if (!app_info)
        {
            // attempt to interpret it as a .desktop file
            app_info = Gio::DesktopAppInfo::create_from_filename(name);
        }
    } else
    {
        // Generate a .desktop file in memory
        auto keyfile = Glib::KeyFile::create();
        keyfile->set_string("Desktop Entry", "Type", "Application");
        keyfile->set_string("Desktop Entry", "Exec", "/bin/sh -c \"" + name + "\"");
        keyfile->set_string("Desktop Entry", "Icon", icon);
        if (label == "")
        {
            label = name;
        }

        keyfile->set_string("Desktop Entry", "Name", label);

        // Hand off to have a custom launcher
        app_info = Gio::DesktopAppInfo::create_from_keyfile(keyfile);
    }

    /* Failed to get a launcher */
    if (!app_info)
    {
        return false;
    }

    button.set_icon_name(icon);
    auto style = button.get_style_context();
    style->add_class("flat");
    style->add_class("launcher");

    button.signal_clicked().connect([=] () { launch(); });
    button.property_scale_factor().signal_changed()
        .connect([=] () {update_icon(); });
    icon_size.set_callback([=] () { update_icon(); });

    update_icon();

    button.set_tooltip_text(app_info->get_name());
    return true;
}

void WfLauncherButton::update_icon()
{
    //set_image_icon(m_icon, app_info->get_icon()->to_string(), icon_size);
    m_icon.set_from_icon_name(app_info->get_icon()->to_string());
}

void WfLauncherButton::launch()
{
    if (app_info)
    {
        app_info->launch(std::vector<Glib::RefPtr<Gio::File>>());
    }
}

WfLauncherButton::WfLauncherButton()
{}
WfLauncherButton::~WfLauncherButton()
{}

static bool begins_with(const std::string& string, const std::string& prefix)
{
    return string.size() >= prefix.size() &&
           string.substr(0, prefix.size()) == prefix;
}

launcher_container WayfireLaunchers::get_launchers_from_config()
{
    auto section = WayfireShellApp::get().config.get_section("panel");
    const std::string desktop_prefix    = "launcher_";
    const std::string file_icon_prefix  = "launcher_icon_";
    const std::string file_cmd_prefix   = "launcher_cmd_";
    const std::string file_label_prefix = "launcher_label_";

    launcher_container launchers;
    auto try_push_launcher = [&launchers] (
        const std::string cmd, const std::string icon, const std::string label = "")
    {
        auto launcher = new WfLauncherButton();
        if (launcher->initialize(cmd, icon, label))
        {
            launchers.push_back(std::unique_ptr<WfLauncherButton>(launcher));
        } else
        {
            delete launcher;
        }
    };

    for (auto opt : section->get_registered_options())
    {
        /* we have a command */
        if (begins_with(opt->get_name(), file_cmd_prefix))
        {
            /* extract launcher name, i.e the string after the prefix */
            auto launcher_name = opt->get_name().substr(file_cmd_prefix.size());
            /* look for the corresponding icon */
            auto icon_option = section->get_option_or(file_icon_prefix + launcher_name);
            if (icon_option)
            {
                /* bingo, found command + icon
                 * now look for the corresponding label  */
                auto label_option = section->get_option_or(file_label_prefix + launcher_name);
                if (label_option)
                {
                    /* found label */
                    try_push_launcher(opt->get_value_str(), icon_option->get_value_str(),
                        label_option->get_value_str());
                } else
                {
                    try_push_launcher(opt->get_value_str(), icon_option->get_value_str());
                }
            }
        }

        /* an entry is a deskop-file entry if the it has the desktop prefix
         * but not the file_icon, file_cmd or file_label prefix */
        if (begins_with(opt->get_name(), desktop_prefix) &&
            !begins_with(opt->get_name(), file_icon_prefix) &&
            !begins_with(opt->get_name(), file_cmd_prefix) &&
            !begins_with(opt->get_name(), file_label_prefix))
        {
            try_push_launcher(opt->get_value_str(), "none");
        }
    }

    return launchers;
}

void WayfireLaunchers::init(Gtk::Box *container)
{
    box.get_style_context()->add_class("launchers");
    container->append(box);
    handle_config_reload();
}

void WayfireLaunchers::handle_config_reload()
{
    for(auto child : box.get_children()){
        box.remove(*child);
    }
    box.set_spacing(WfOption<int>{"panel/launchers_spacing"});

    launchers = get_launchers_from_config();
    for (auto& l : launchers)
    {
        box.append(l->button);
    }

}
