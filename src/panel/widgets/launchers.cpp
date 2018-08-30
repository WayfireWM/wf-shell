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
        return theme->load_icon(icon, size)->scale_simple(size, size, Gdk::INTERP_BILINEAR);
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

void WfLauncherButton::set_size(int size)
{
    std::cout << "set size " << size << std::endl;
    this->current_size = size;

    /* set button spacing */
    evbox.set_margin_top((panel_size - size) / 2);
    evbox.set_margin_bottom((panel_size - size) / 2);

    evbox.set_margin_left((panel_size - size) / 2);
    evbox.set_margin_right((panel_size - size + 1) / 2);

    // initial scale
    on_scale_update();
}

bool WfLauncherButton::initialize(wayfire_config *config, std::string name,
                                  std::string icon)
{
    launcher_name = name;

    panel_size = *config->get_section("panel")->get_option("panel_thickness", "48");
    int default_size = panel_size * 0.7;
    base_size = *config->get_section("panel")->get_option("launcher_size", std::to_string(default_size));
    base_size = std::min(base_size, panel_size);

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

    evbox.add(image);
    evbox.signal_button_press_event().connect(sigc::mem_fun(this, &WfLauncherButton::on_click));
    evbox.signal_button_release_event().connect(sigc::mem_fun(this, &WfLauncherButton::on_click));
    evbox.signal_enter_notify_event().connect(sigc::mem_fun(this, &WfLauncherButton::on_enter));
    evbox.signal_leave_notify_event().connect(sigc::mem_fun(this, &WfLauncherButton::on_leave));

    evbox.signal_draw().connect(sigc::mem_fun(this, &WfLauncherButton::on_draw));

    set_size(base_size);

    hover_animation = wf_duration(new_static_option("300"));
    hover_animation.start(base_size, base_size);

    evbox.property_scale_factor().signal_changed()
        .connect(sigc::mem_fun(this, &WfLauncherButton::on_scale_update));


    return true;
}

bool WfLauncherButton::on_click(GdkEventButton *ev)
{
    assert(info);

    std::cout << "on click" << std::endl;
    if (ev->button == 1 && ev->type == GDK_BUTTON_RELEASE)
    {
        info->execute();
        if (!hover_animation.running())
            on_leave(NULL);
    }

    if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS)
    {
        /* touch will generate button_press, but not enter notify */
        if (!hover_animation.running())
            on_enter(NULL);
    }

    return true;
}

bool WfLauncherButton::on_enter(GdkEventCrossing* ev)
{
    int current_size = hover_animation.progress();
    int target_size = std::min((double)panel_size, base_size * 1.2);

    evbox.queue_draw();
    std::cout << "start animation from " << current_size << " to " << target_size << std::endl;
    hover_animation.start(current_size, target_size);
    animation_running = true;

    return false;
}

bool WfLauncherButton::on_leave(GdkEventCrossing *ev)
{
    int current_size = hover_animation.progress();
    evbox.queue_draw();
    hover_animation.start(current_size, base_size);
    std::cout << "start animation from " << current_size << " to " << base_size << std::endl;
    animation_running = true;

    return false;
}

bool WfLauncherButton::on_draw(const Cairo::RefPtr<Cairo::Context>& ctx)
{
    if (animation_running)
    {
        set_size(hover_animation.progress());
        animation_running = hover_animation.running();
    }

    return false;
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
    auto ptr_pbuff = info->get_pixbuf(current_size * image.get_scale_factor());

    std::cout << "got pbuff with " << ptr_pbuff->get_width() << "x" << ptr_pbuff->get_height() << std::endl;


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

    box.set_margin_left(*config->get_section("panel")->get_option("launchers_margin_left", "12"));
    box.set_margin_right(*config->get_section("panel")->get_option("launchers_margin_right", "0"));
    box.set_spacing(*config->get_section("panel")->get_option("launchers_spacing", "6"));

    this->launchers = get_launchers_from_config(config);
    for (auto launcher : launchers)
        box.pack_start(launcher->evbox, false, false);
}
