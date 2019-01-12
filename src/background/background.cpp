#include <gtkmm/window.h>
#include <gtkmm/image.h>
#include <gdkmm/pixbuf.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>

class WayfireBackground
{
    WayfireShellApp *app;
    WayfireOutput *output;
    zwf_wm_surface_v1 *wm_surface = NULL;

    Gtk::Window window;
    Gtk::Image image;

    bool inhibited = true;

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
            ZWF_OUTPUT_V1_WM_ROLE_BACKGROUND);
        zwf_wm_surface_v1_configure(wm_surface, 0, 0);
    }

    int output_width, output_height;
    void handle_output_resize(uint32_t width, uint32_t height)
    {
        output_width = width;
        output_height = height;

        window.set_size_request(width, height);
        window.show_all();

        if (!wm_surface)
            create_wm_surface();

        set_background();
    }

    wf_option background_image;
    wf_option_callback image_updated;

    void set_background()
    {
        auto path = background_image->as_string();
        try {
            auto pbuf = Gdk::Pixbuf::create_from_file(path);
            int scale = window.get_scale_factor();
            pbuf = pbuf->scale_simple(output_width * scale,
                                      output_height * scale,
                                      Gdk::INTERP_BILINEAR);

            set_image_pixbuf(image, pbuf, scale);
            if (!image.get_parent())
            {
                window.add(image);
                window.show_all();
            }
        } catch (...)
        {
            window.remove();
            if (path != "none")
                std::cerr << "Failed to load background image " << path << std::endl;
        }

        if (inhibited)
        {
            zwf_output_v1_inhibit_output_done(output->zwf);
            inhibited = false;
        }
    }

    void setup_window()
    {
        window.set_resizable(false);
        window.set_decorated(false);

        background_image = app->config->get_section("background")
            ->get_option("image", "none");
        image_updated = [=] () { set_background(); };
        background_image->add_updated_handler(&image_updated);

        window.property_scale_factor().signal_changed().connect(
            sigc::mem_fun(this, &WayfireBackground::set_background));
    }

    public:
    WayfireBackground(WayfireShellApp *app, WayfireOutput *output)
    {
        this->app = app;
        this->output = output;

        zwf_output_v1_inhibit_output(output->zwf);
        setup_window();
        output->resized_callback = [=] (WayfireOutput*, uint32_t w, uint32_t h)
        {
            std::cout << "handle resize" << std::endl;
            handle_output_resize(w, h);
        };
    }

    ~WayfireBackground()
    {
        background_image->rem_updated_handler(&image_updated);
    }
};

class WayfireBackgroundApp : public WayfireShellApp
{
    std::map<WayfireOutput*, std::unique_ptr<WayfireBackground> > backgrounds;

    public:
    WayfireBackgroundApp(int argc, char **argv)
        : WayfireShellApp(argc, argv)
    {
    }

    void on_new_output(WayfireOutput *output)
    {
        backgrounds[output] = std::unique_ptr<WayfireBackground> (
            new WayfireBackground(this, output));
    }

    void on_output_removed(WayfireOutput *output)
    {
        backgrounds.erase(output);
    }
};

int main(int argc, char **argv)
{
    WayfireBackgroundApp background_app(argc, argv);
    background_app.run();
    return 0;
}
