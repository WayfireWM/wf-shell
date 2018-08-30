#include "launchers.hpp"
#include <giomm/file.h>
#include <glibmm/spawn.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/icontheme.h>
#include <gdk/gdkcairo.h>
#include <cassert>
#include <iostream>
#include <config.hpp>

// create launcher from a .desktop file or app-id
struct DesktopLauncherInfo : public LauncherInfo
{
    Glib::RefPtr<Gio::DesktopAppInfo> app_info;
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
        return true;
    }

    Glib::RefPtr<Gdk::Pixbuf> get_pixbuf(int32_t size)
    {
        auto icon = app_info->get_icon()->to_string();
        auto theme = Gtk::IconTheme::get_default();
        return theme->load_icon(icon, size);
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
    std::string command;
    std::string icon;

    bool load(std::string name, std::string icon)
    {
        command = name;
        this->icon = icon;

        try {
            // check if file is loadable
            Gdk::Pixbuf::create_from_file(icon, 24, 24);
        } catch(...) {
            return false;
        }

        return true;
    }

    Glib::RefPtr<Gdk::Pixbuf> get_pixbuf(int32_t size)
    {
        return Gdk::Pixbuf::create_from_file(icon, size, size);
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

bool WfLauncherButton::initialize(wayfire_config *config, std::string name,
                                  std::string icon)
{
    launcher_name = name;

    int32_t default_size = *config->get_section("panel")->get_option("panel_thickness", "48");
    default_size = default_size * 0.8;
    size = *config->get_section("panel")->get_option("launcher_size", std::to_string(default_size));

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

    button.add(image);
    button.signal_clicked().connect(sigc::mem_fun(this, &WfLauncherButton::on_click));

    // initial scale
    on_scale_update();

    button.property_scale_factor().signal_changed()
        .connect(sigc::mem_fun(this, &WfLauncherButton::on_scale_update));

    return true;
}

void WfLauncherButton::on_click()
{
    assert(info);
    info->execute();
}

/* Because icons can have different sizes, we need to use a Gdk::Pixbuf
 * to convert them to the appropriate size. However, Gdk::Pixbuf operates
 * in absolute pixel size, so this doesn't work nicely with scaled outputs.
 *
 * To get around the problem, we first create the Pixbuf with a scaled size,
 * then convert it to a cairo_surface with the appropriate scale, and use this
 * cairo surface as the source for the Gtk::Image */
void WfLauncherButton::on_scale_update()
{
    int scale = image.get_scale_factor();

    // hold a reference to the RefPtr
    auto ptr_pbuff = info->get_pixbuf(size * image.get_scale_factor());
    auto pbuff = ptr_pbuff->gobj();
    auto cairo_surface = gdk_cairo_surface_create_from_pixbuf(pbuff, scale, NULL);

    gtk_image_set_from_surface(image.gobj(), cairo_surface);
    cairo_surface_destroy(cairo_surface);
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
    auto try_push_launcher = [&launchers, config] (const std::string cmd,
                                                   const std::string icon)
    {
        auto launcher = new WfLauncherButton();
        if (launcher->initialize(config, cmd, icon))
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
    container->pack_start(box, false, false);
    box.set_spacing(12);

    this->launchers = get_launchers_from_config(config);
    for (auto launcher : launchers)
        box.pack_start(launcher->button, false, false);
}
