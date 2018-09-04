#include <glibmm/main.h>
#include <glibmm/iochannel.h>
#include <gtkmm/window.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/application.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>

#include <stdio.h>
#include <iostream>
#include <sys/inotify.h>

#include "widgets/battery.hpp"
#include "widgets/clock.hpp"
#include "widgets/launchers.hpp"
#include "display.hpp"

class WayfirePanel;
namespace
{
    int inotify_fd;

    WayfireDisplay *display = NULL;
    wayfire_config *panel_config;

    std::vector<std::unique_ptr<WayfirePanel>> panels;
}

class WayfirePanel
{
    Gtk::Window window;

    Gtk::HBox content_box;
    Gtk::HBox left_box, center_box, right_box;

    WayfireWidget *widget, *widget1, *widget2;
    WayfireOutput *output;
    zwf_wm_surface_v1 *wm_surface = NULL;

    void create_wm_surface()
    {
        auto gdk_window = window.get_window()->gobj();
        auto surface = gdk_wayland_window_get_wl_surface(gdk_window);

        if (!surface)
        {
            std::cerr << "Error: created window was not a wayland surface" << std::endl;
            std::exit(-1);
        }

        wm_surface = zwf_output_v1_get_wm_surface(output->zwf, surface,
                                                  ZWF_OUTPUT_V1_WM_ROLE_PANEL);
        zwf_wm_surface_v1_configure(wm_surface, 0, 40);
        zwf_wm_surface_v1_set_exclusive_zone(
            wm_surface, ZWF_WM_SURFACE_V1_ANCHOR_EDGE_TOP,
            window.get_height());
    }

    void handle_resize(uint32_t width, uint32_t height)
    {
        uint32_t panel_height = height * 0.05;
        window.resize(width, panel_height);
        window.set_size_request(width, panel_height);
        window.set_default_size(width, panel_height);
        window.show_all();

        if (!wm_surface)
            create_wm_surface();
    }

    void setup_window()
    {
        window.set_resizable(false);
        window.set_decorated(false);
        auto bg_color = panel_config->get_section("panel")
            ->get_option("background_color", "gtk_default");

        if (bg_color->as_string() != "gtk_default")
        {
            Gdk::RGBA rgba;
            auto color_string = bg_color->as_string();

            /* see if it is in #XXXXXX format */
            if (color_string.size() && color_string[0] == '#')
            {
                rgba.set(color_string);
            } else {
                /* otherwise, simply a list of double values, parse by default */
                auto color = bg_color->as_color();
                rgba.set_red(color.r);
                rgba.set_green(color.g);
                rgba.set_blue(color.b);
                rgba.set_alpha(color.a);
            }

            window.override_background_color(rgba);
        }
    }

    void init_layout()
    {
        content_box.pack_start(left_box, false, false);
        content_box.set_center_widget(center_box);
        content_box.pack_end(right_box, false, false);
        window.add(content_box);
    }

    void init_widgets()
    {
        widget = new WayfireClock();
        widget->init(&center_box, panel_config);

        widget1 = new WayfireLaunchers();
        widget1->init(&left_box, panel_config);

        widget2 = new WayfireBatteryInfo();
        widget2->init(&right_box, panel_config);
    }

    public:
    WayfirePanel(WayfireOutput *output)
    {
        this->output = output;

        setup_window();
        init_layout();
        init_widgets();

        output->resized_callback = [=] (WayfireOutput*, uint32_t w, uint32_t h)
        {
            handle_resize(w, h);
        };

        output->destroyed_callback = [=] (WayfireOutput *output)
        {
            delete this;
        };
    }

    void handle_config_reload()
    {
        widget->handle_config_reload(panel_config);
        widget1->handle_config_reload(panel_config);
        widget2->handle_config_reload(panel_config);
    }
};

std::string get_config_file()
{
    std::string home_dir = secure_getenv("HOME");
    std::string config_file = home_dir + "/.config/wf-shell.ini";
    return config_file;
}

#define INOT_BUF_SIZE (1024 * sizeof(inotify_event))
char buf[INOT_BUF_SIZE];

static void reload_config(int fd)
{
    panel_config->reload_config();

    for (auto& panel : panels)
        panel->handle_config_reload();

    inotify_add_watch(fd, get_config_file().c_str(), IN_MODIFY);
}

static bool on_config_reload(Glib::IOCondition)
{
    /* read, but don't use */
    read(inotify_fd, buf, INOT_BUF_SIZE);
    reload_config(inotify_fd);

    return true;
}

void on_activate(Glib::RefPtr<Gtk::Application> app)
{
    app->hold();

    auto handle_new_output = [] (WayfireOutput *output)
    {
        panels.push_back(std::unique_ptr<WayfirePanel> (new WayfirePanel(output)));
    };

    panel_config = new wayfire_config(get_config_file());

    inotify_fd = inotify_init();
    reload_config(inotify_fd);

    Glib::signal_io().connect(sigc::ptr_fun(&on_config_reload),
                              inotify_fd, Glib::IO_IN | Glib::IO_HUP);

    display = new WayfireDisplay(handle_new_output);
}

int main(int argc, char **argv)
{
    auto app = Gtk::Application::create(argc, argv);
    app->signal_activate().connect(sigc::bind(&on_activate, app));
    app->run();

    if (display)
        delete display;

    return 0;
}
