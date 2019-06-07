#include <wordexp.h>
#include <glibmm/main.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <gtkmm/image.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/general.h>
#include <gdk/gdkwayland.h>
#include <config.hpp>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>
#include <animation.hpp>

#include "background.hpp"


void
BackgroundDrawingArea::show_image(Glib::RefPtr<Gdk::Pixbuf> image, double offset_x, double offset_y)
{
    if (!image)
    {
        to_image.pbuf.clear();
        from_image.pbuf.clear();
        return;
    }

    from_image = to_image;
    to_image.pbuf = image;
    to_image.x = offset_x;
    to_image.y = offset_y;

    queue_draw();
    fade.start(0.0, 1.0);
}

bool
BackgroundDrawingArea::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    auto alpha = fade.progress();

    if (fade.running())
        queue_draw();

    Gdk::Cairo::set_source_pixbuf(cr, to_image.pbuf, to_image.x, to_image.y);
    cr->rectangle(0, 0, to_image.pbuf->get_width(), to_image.pbuf->get_height());
    cr->paint_with_alpha(alpha);

    if (!from_image.pbuf)
        return false;

    Gdk::Cairo::set_source_pixbuf(cr, from_image.pbuf, from_image.x, from_image.y);
    cr->rectangle(0, 0, from_image.pbuf->get_width(), from_image.pbuf->get_height());
    cr->paint_with_alpha(1.0 - alpha);

    return false;
}

BackgroundDrawingArea::BackgroundDrawingArea()
{
    fade = wf_duration(new_static_option("1000"), wf_animation::linear);
    fade.start(0.0, 0.0);
}

Glib::RefPtr<Gdk::Pixbuf>
WayfireBackground::create_from_file_safe(std::string path)
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;
    int width = output_width * scale;
    int height = output_height * scale;
    bool preserve_aspect = background_preserve_aspect->as_int() ? true : false;

    try
    {
        pbuf = Gdk::Pixbuf::create_from_file(path, width, height, preserve_aspect);
        if (preserve_aspect)
        {
            bool eq_width = (width == pbuf->get_width());
            offset_x = eq_width ? 0 : (width - pbuf->get_width()) * 0.5;
            offset_y = eq_width ? (height - pbuf->get_height()) * 0.5 : 0;
        }
        else
        {
            offset_x = offset_y = 0.0;
        }
        return pbuf;
    }
    catch (...)
    {
        return pbuf;
    }
}

void
WayfireBackground::create_wm_surface()
{
    auto gdk_window = window.get_window()->gobj();
    auto surface = gdk_wayland_window_get_wl_surface(gdk_window);

    if (!surface)
    {
        std::cerr << "Error: created window was not a wayland surface" << std::endl;
        std::exit(-1);
    }

    wm_surface = zwf_shell_manager_v1_get_wm_surface(
        output->display->zwf_shell_manager, surface,
        ZWF_WM_SURFACE_V1_ROLE_BACKGROUND, output->handle);
    zwf_wm_surface_v1_configure(wm_surface, 0, 0);
}

void
WayfireBackground::handle_output_resize(uint32_t width, uint32_t height)
{
    output_width = width;
    output_height = height;

    window.set_size_request(width, height);
    window.show_all();

    if (!wm_surface)
        create_wm_surface();

    set_background();
}

bool
WayfireBackground::change_background(int timer)
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;
    std::string path;

    if (!load_next_background(pbuf, path))
        return false;

    std::cout << "Loaded " << path << std::endl;

    drawing_area.show_image(pbuf, offset_x, offset_y);

    return true;
}

bool
WayfireBackground::load_images_from_dir(std::string path)
{
    wordexp_t exp;

    /* Expand path */
    wordexp(path.c_str(), &exp, 0);
    auto dir = opendir(exp.we_wordv[0]);
    if (!dir)
        return false;

    /* Iterate over all files in the directory */
    dirent *file;
    while ((file = readdir(dir)) != 0)
    {
        /* Skip hidden files and folders */
        if (file->d_name[0] == '.')
            continue;

        auto fullpath = std::string(exp.we_wordv[0]) + "/" + file->d_name;

        struct stat next;
        if (stat(fullpath.c_str(), &next) == 0)
        {
            if (S_ISDIR(next.st_mode))
            {
                /* Recursive search */
                load_images_from_dir(fullpath);
            }
            else
            {
                images.push_back(fullpath);
            }
        }
    }

    if (background_randomize->as_int() && images.size())
    {
        srand(time(0));
        std::random_shuffle(images.begin(), images.end());
    }

    return true;
}

bool
WayfireBackground::load_next_background(Glib::RefPtr<Gdk::Pixbuf> &pbuf, std::string &path)
{
    while (!pbuf)
    {
        if (!images.size())
        {
            std::cerr << "Failed to load background images from " << background_image->as_string() << std::endl;
            window.remove();
            return false;
        }

        current_background = (current_background + 1) % images.size();

        path = images[current_background];
        pbuf = create_from_file_safe(path);

        if (!pbuf)
            images.erase(images.begin() + current_background);
    }

    return true;
}

void
WayfireBackground::reset_background()
{
    images.clear();
    current_background = 0;
    change_bg_conn.disconnect();
    scale = window.get_scale_factor();
}

void
WayfireBackground::set_background()
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;

    reset_background();

    auto path = background_image->as_string();
    int cycle_timeout = background_cycle_timeout->as_int() * 1000;
    try {
        if (load_images_from_dir(path) && images.size())
        {
            if (!load_next_background(pbuf, path))
                throw std::exception();

            std::cout << "Loaded " << path << std::endl;
        }
        else
        {
            pbuf = create_from_file_safe(path);
            if (!pbuf)
                throw std::exception();
        }

        if (!drawing_area.get_parent())
        {
            window.add(drawing_area);
            window.show_all();
        }
        if (images.size())
        {
            change_bg_conn = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(
                this, &WayfireBackground::change_background), 0), cycle_timeout);
        }
    } catch (...)
    {
        window.remove();
        if (images.size())
        {
            std::cerr << "Failed to load background images from " << path << std::endl;
        }
        else if (path != "none")
        {
            std::cerr << "Failed to load background image " << path << std::endl;
        }
    }

    drawing_area.show_image(pbuf, offset_x, offset_y);

    if (inhibited)
    {
        zwf_output_v1_inhibit_output_done(output->zwf);
        inhibited = false;
    }
}

void
WayfireBackground::reset_cycle_timeout()
{
    int cycle_timeout = background_cycle_timeout->as_int() * 1000;
    change_bg_conn.disconnect();
    if (images.size())
    {
        change_bg_conn = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(
            this, &WayfireBackground::change_background), 0), cycle_timeout);
    }
}

void
WayfireBackground::setup_window()
{
    window.set_resizable(false);
    window.set_decorated(false);

    background_image = app->config->get_section("background")
        ->get_option("image", "none");
    background_cycle_timeout = app->config->get_section("background")
        ->get_option("cycle_timeout", "150");
    background_randomize = app->config->get_section("background")
        ->get_option("randomize", "0");
    background_preserve_aspect = app->config->get_section("background")
        ->get_option("preserve_aspect", "0");
    init_background = [=] () { set_background(); };
    cycle_timeout_updated = [=] () { reset_cycle_timeout(); };
    background_image->add_updated_handler(&init_background);
    background_cycle_timeout->add_updated_handler(&cycle_timeout_updated);
    background_randomize->add_updated_handler(&init_background);
    background_preserve_aspect->add_updated_handler(&init_background);

    window.property_scale_factor().signal_changed().connect(
        sigc::mem_fun(this, &WayfireBackground::set_background));
}

WayfireBackground::WayfireBackground(WayfireShellApp *app, WayfireOutput *output)
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

WayfireBackground::~WayfireBackground()
{
    background_image->rem_updated_handler(&init_background);
    background_cycle_timeout->rem_updated_handler(&cycle_timeout_updated);
    background_randomize->rem_updated_handler(&init_background);
    background_preserve_aspect->rem_updated_handler(&init_background);
}

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
