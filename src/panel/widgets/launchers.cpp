#include "launchers.hpp"
#include <giomm/file.h>
#include <glibmm/spawn.h>
#include <cassert>
#include <iostream>
#include <config.hpp>

// create launcher from a .desktop file or app-id
struct DesktopLauncherInfo : public LauncherInfo
{
    Glib::RefPtr<Gio::DesktopAppInfo> app_info;
    Gtk::Image image;

    bool load(std::string name)
    {
        // attempt from App-ID
        app_info = Gio::DesktopAppInfo::create(name);
        if (!app_info)
        {
            // attempt to interpret it as a .desktop file
            app_info = Gio::DesktopAppInfo::create_from_filename(name);
        }

        // neither interpretation succeeded
        if (!app_info)
            return false;

        Glib::RefPtr<const Gio::Icon> icon = app_info->get_icon();
        image.set(icon, Gtk::ICON_SIZE_INVALID);
        return true;
    }

    Gtk::Image& get_image()
    {
        return image;
    }

    std::string get_text()
    {
        return app_info->get_name();
    }

    void execute()
    {
        app_info->launch(std::vector<Glib::RefPtr<Gio::File>>());
    }

    virtual ~DesktopLauncherInfo() {}
};

// create a launcher from a command + icon
struct FileLauncherInfo : public LauncherInfo
{
    Gtk::Image image;
    std::string command;

    bool load(std::string name, std::string icon)
    {
        command = name;
        image = Gtk::Image(icon);
        return true;
    }

    Gtk::Image& get_image()
    {
        return image;
    }

    std::string get_text()
    {
        return command;
    }

    void execute()
    {
        Glib::spawn_command_line_async("/bin/bash -c \'" + command + "\'");
    }

    virtual ~FileLauncherInfo() {}
};

bool WfLauncherButton::initialize(std::string name, std::string icon)
{
    launcher_name = name;

    if (icon == "none")
    {
        auto dl = new DesktopLauncherInfo();
        if (!dl->load(name))
        {
            std::cerr << "Failed to load info for " << name << std::endl;
            return false;
        }
        info = dl;
    } else
    {
        auto fl = new FileLauncherInfo();
        if (!fl->load(name, icon))
        {
            std::cerr << "Failed to load icon " << icon << std::endl;
            return false;
        }
        info = fl;
    }

    button.set_image(info->get_image());
    button.signal_clicked().connect(sigc::mem_fun(this, &WfLauncherButton::on_click));

    return true;
}

void WfLauncherButton::on_click()
{
    assert(info);
    info->execute();
}

WfLauncherButton::~WfLauncherButton()
{
    delete info;
}

static bool begins_with(const std::string& string, const std::string& prefix)
{
    return string.size() >= prefix.size() &&
        string.substr(0, prefix.size()) == prefix;
}

launcher_container WayfireLaunchers::get_launchers_from_config(wayfire_config *config)
{
    auto section = config->get_section("panel");

    const std::string desktop_prefix   = "launcher_";
    const std::string file_icon_prefix = "launcher_icon_";
    const std::string file_cmd_prefix = "launcher_cmd_";

    launcher_container launchers;
    auto try_push_launcher = [&launchers] (const std::string cmd,
                                           const std::string icon)
    {
        auto launcher = new WfLauncherButton();
        if (launcher->initialize(cmd, icon))
        {
            launchers.push_back(launcher);
        } else
        {
            delete launcher;
        }
    };

    auto options = section->options;
    for (auto opt : options)
    {
        /* we have a command */
        if (begins_with(opt->name, file_cmd_prefix))
        {
            /* extract launcher name, i.e the string after the prefix */
            auto launcher_name = opt->name.substr(file_cmd_prefix.size());
            /* look for the corresponding icon */
            auto icon_option = section->get_option(file_icon_prefix + launcher_name, "");

            std::cout << "look for " << launcher_name << std::endl;
            if (icon_option->as_string() != "")
            {
                std::cout << "no such icon" << std::endl;
                /* bingo, found command + icon */
                try_push_launcher(opt->as_string(), icon_option->as_string());
            }
        }

        /* an entry is a deskop-file entry if the it has the desktop prefix
         * but not the file_icon or file_cmd prefix */
        if (begins_with(opt->name, desktop_prefix) &&
            !begins_with(opt->name, file_icon_prefix) &&
            !begins_with(opt->name, file_cmd_prefix))
        {
            try_push_launcher(opt->as_string(), "none");
        }
    }

    return launchers;
}

void WayfireLaunchers::init(Gtk::HBox *container, wayfire_config *config)
{
    container->pack_end(box, false, false);

    this->launchers = get_launchers_from_config(config);
    for (auto launcher : launchers)
        box.pack_start(launcher->button, false, false);
}
