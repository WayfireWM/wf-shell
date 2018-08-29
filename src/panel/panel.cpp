#include <gtkmm/window.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/application.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>

#include <iostream>

#include "widgets/clock.hpp"
#include "widgets/launchers.hpp"
#include "display.hpp"

namespace
{
    WayfireDisplay *display = NULL;
    wayfire_config *panel_config;
}

class WayfirePanel
{
    Gtk::Window window;

    Gtk::HBox content_box;
    Gtk::HBox left_box, center_box, right_box;

    WayfireWidget *widget, *widget1;
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
        zwf_wm_surface_v1_configure(wm_surface, 0, 0);
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
};

void on_activate(Glib::RefPtr<Gtk::Application> app)
{
    app->hold();

    auto handle_new_output = [] (WayfireOutput *output)
    {
        std::cout << "new output" << std::endl;
        new WayfirePanel(output);
    };

    std::string home_dir = secure_getenv("HOME");
    std::string config_file = home_dir + "/.config/wf-shell.ini";

    panel_config = new wayfire_config(config_file);
    panel_config->reload_config(); // initial loading of config file

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
