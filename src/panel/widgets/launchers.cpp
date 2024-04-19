#include "launchers.hpp"
#include <giomm/file.h>
#include <glibmm/spawn.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/general.h>
#include <gtkmm/icontheme.h>
#include <gdk/gdkcairo.h>
#include <cassert>
#include <iostream>
#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>

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
        {
            return false;
        }

        return true;
    }

    std::string get_icon()
    {
        return app_info->get_icon()->to_string();
    }

    std::string get_text()
    {
        return app_info->get_name();
    }

    void execute()
    {
        app_info->launch(std::vector<Glib::RefPtr<Gio::File>>());
    }

    virtual ~DesktopLauncherInfo()
    {}
};

// create a launcher from a command + icon
struct FileLauncherInfo : public LauncherInfo
{
    std::string command;
    std::string icon;
    std::string label;

    bool load(std::string command, std::string icon, std::string label)
    {
        this->command = command;
        this->icon    = icon;
        if (label == "")
        {
            this->label = command;
        } else
        {
            this->label = label;
        }

        return load_icon_pixbuf_safe(icon, 24).get() != nullptr;
    }

    std::string get_icon()
    {
        return icon;
    }

    std::string get_text()
    {
        return label;
    }

    void execute()
    {
        Glib::spawn_command_line_async("/bin/bash -c \'" + command + "\'");
    }

    virtual ~FileLauncherInfo()
    {}
};

bool WfLauncherButton::initialize(std::string name, std::string icon, std::string label)
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
        if (!fl->load(name, icon, label))
        {
            std::cerr << "Failed to load icon " << icon << std::endl;
            return false;
        }

        info = fl;
    }

    evbox.signal_button_press_event().connect(sigc::mem_fun(this, &WfLauncherButton::on_click));
    evbox.signal_button_release_event().connect(sigc::mem_fun(this, &WfLauncherButton::on_click));
    evbox.signal_enter_notify_event().connect(sigc::mem_fun(this, &WfLauncherButton::on_enter));
    evbox.signal_leave_notify_event().connect(sigc::mem_fun(this, &WfLauncherButton::on_leave));

    evbox.signal_draw().connect(
        [this] (const Cairo::RefPtr<Cairo::Context> context) -> bool
    {
        if (image)
        {
            auto width  = evbox.get_allocated_width();
            auto height = evbox.get_allocated_height();

            /* Center in widget space */
            double offset_x = (width - current_size) / 2;
            double offset_y = (height - current_size) / 2;
            context->translate(offset_x, offset_y);

            auto pix_width  = image->get_width();
            auto pix_height = image->get_height();

            /* Scale to image space */
            context->scale(current_size / pix_width, current_size / pix_height);

            Gdk::Cairo::set_source_pixbuf(context, image, 0, 0);
            context->rectangle(0, 0, pix_width, pix_height);
            context->fill();
            if (current_size.running())
            {
                evbox.queue_draw();
            }
        }

        return false;
    });

    evbox.property_scale_factor().signal_changed().connect(
        [=] () { update_size(); });

    size.set_callback([=] () { update_size(); });

    update_size();

    evbox.set_tooltip_text(info->get_text());
    return true;
}

void WfLauncherButton::update_size()
{
    base_size = size / LAUNCHERS_ICON_SCALE;
    image     = get_pixbuf(info->get_icon(), size);

    current_size.set(base_size, base_size);
    evbox.set_size_request(size);
}

bool WfLauncherButton::on_click(GdkEventButton *ev)
{
    assert(info);
    if ((ev->button == 1) && (ev->type == GDK_BUTTON_RELEASE))
    {
        info->execute();
        if (!current_size.running())
        {
            on_leave(NULL);
        }
    }

    if ((ev->button == 1) && (ev->type == GDK_BUTTON_PRESS))
    {
        /* touch will generate button_press, but not enter notify */
        if (!current_size.running())
        {
            on_enter(NULL);
        }
    }

    return true;
}

Glib::RefPtr<Gdk::Pixbuf> WfLauncherButton::get_pixbuf(std::string icon, int32_t size)
{
    size = size * evbox.get_scale_factor();
    std::string absolute_path = "/";
    if (!icon.compare(0, absolute_path.size(), absolute_path))
    {
        auto image = Gdk::Pixbuf::create_from_file(icon, size, size);
        if (image)
        {
            return image;
        }
    }

    auto theme = Gtk::IconTheme::get_default();
    if (theme->lookup_icon(icon, size))
    {
        return theme->load_icon(icon, size)
            ->scale_simple(size, size, Gdk::INTERP_BILINEAR);
    }

    std::cerr << "Failed to load icon \"" << icon << "\"" << std::endl;
    if (theme->lookup_icon(icon, size))
    {
        return theme->load_icon("image-missing", size)
            ->scale_simple(size, size, Gdk::INTERP_BILINEAR);
    }

    return Glib::RefPtr<Gdk::Pixbuf>();
}

static int get_animation_duration(int start, int end, int scale)
{
    return WfOption<int>{"panel/launchers_animation_duration"}.value();
}

bool WfLauncherButton::on_enter(GdkEventCrossing *ev)
{
    int duration = get_animation_duration(
        current_size, size, evbox.get_scale_factor());

    evbox.queue_draw();
    current_size = LauncherAnimation{wf::create_option(duration),
        (int)current_size, size};
    return false;
}

bool WfLauncherButton::on_leave(GdkEventCrossing *ev)
{
    evbox.queue_draw();
    int duration = get_animation_duration(
        current_size, base_size, evbox.get_scale_factor());

    current_size = LauncherAnimation{wf::create_option(duration),
        (int)current_size, base_size};

    return false;
}

WfLauncherButton::WfLauncherButton()
{}
WfLauncherButton::~WfLauncherButton()
{
    delete info;
}

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
                /* bingo, found command + icon now look for the corresponding label  */
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

        /* an entry is a deskop-file entry if the it has the desktop prefix but not the file_icon, file_cmd or
         * file_label prefix */
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

void WayfireLaunchers::init(Gtk::HBox *container)
{
    container->pack_start(box, false, false);
    handle_config_reload();
}

void WayfireLaunchers::handle_config_reload()
{
    box.set_spacing(WfOption<int>{"panel/launchers_spacing"});

    launchers = get_launchers_from_config();
    for (auto& l : launchers)
    {
        box.pack_start(l->evbox, false, false);
    }

    box.show_all();
}
