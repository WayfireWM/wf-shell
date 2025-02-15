#include <wordexp.h>
#include <glibmm/main.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <gtkmm/image.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/general.h>
#include <gdk/wayland/gdkwayland.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <random>
#include <algorithm>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <gtk4-layer-shell.h>
#include <glib-unix.h>

#include "background.hpp"

Glib::RefPtr<BackgroundImageAdjustments> BackgroundImage::generate_adjustments(int width, int height)
{
    // Sanity checks
    if (width == 0                ||
        height == 0               ||
        source == nullptr         ||
        source->get_width() ==0   ||
        source->get_height() == 0 )
    {
        return nullptr;
    }

    double screen_width = (double)width;
    double screen_height = (double)height;
    double source_width = (double)source->get_width();
    double source_height = (double)source->get_height();

    auto adjustment = Glib::RefPtr<BackgroundImageAdjustments>(new BackgroundImageAdjustments());
    std::string fill_and_crop_string = "fill_and_crop";
    std::string stretch_string = "stretch";
    if (!stretch_string.compare(fill_type))
    {
        adjustment->x = 0.0;
        adjustment->y = 0.0;
        adjustment->scale_y = screen_height / source_height;
        adjustment->scale_x = screen_width / source_width;
        return adjustment;
    }else if (!fill_and_crop_string.compare(fill_type))
    {
        double screen_aspect_ratio = screen_width / screen_height;
        double image_aspect_ratio  = source_width / source_height;
        bool should_fill_width    = (screen_aspect_ratio > image_aspect_ratio);
        if (should_fill_width)
        {
            adjustment->scale_x = screen_width / source_width;
            adjustment->scale_y = screen_width / source_width;
            adjustment->y    = ((screen_height / adjustment->scale_x) - source_height) * 0.5;
            adjustment->x    = 0.0;
        } else
        {
            adjustment->scale_x = screen_height / source_height;
            adjustment->scale_y = screen_height / source_height;
            adjustment->x    = ((screen_width / adjustment->scale_x) - source_width) * 0.5;
            adjustment->y    = 0.0;
        }
        return adjustment;
    } else
    {
        double screen_aspect_ratio = screen_width / screen_height;
        double image_aspect_ratio  = source_width / source_height;
        bool should_fill_width    = (screen_aspect_ratio > image_aspect_ratio);
        if (should_fill_width)
        {
            adjustment->scale_x = screen_height / source_height;
            adjustment->scale_y = screen_height / source_height;
            adjustment->y    = ((screen_height / adjustment->scale_x) - source_height) * 0.5;
            adjustment->x    = ((screen_width / adjustment->scale_x) - source_width) * 0.5;
        } else
        {
            adjustment->scale_x = screen_width / source_width;
            adjustment->scale_y = screen_width / source_width;
            adjustment->x    = ((screen_width / adjustment->scale_x) - source_width) * 0.5;
            adjustment->y    = ((screen_height / adjustment->scale_x) - source_height) * 0.5;
        }
        return adjustment;
    }
    return nullptr;
}

void BackgroundDrawingArea::show_image(Glib::RefPtr<BackgroundImage> next_image)
{
    if (!next_image)
    {
        to_image = nullptr;
        from_image = nullptr;
        return;
    }

    from_image = to_image;
    to_image = next_image;
    std::cout << fade_duration << std::endl;
    fade = {
        fade_duration,
        wf::animation::smoothing::linear
    };

    fade.animate(0.0, 1.0);

    Glib::signal_timeout().connect([=] ()
    {
        this->queue_draw();
        return fade.running();
    }, 16);
}

bool BackgroundDrawingArea::do_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height)
{
    if (!to_image)
    {
        return false;
    }

    auto to_adjustments = to_image->generate_adjustments(width, height);
    if (to_adjustments == nullptr)
    {
        return false;
    }
    cr->save();
    cr->scale(to_adjustments->scale_x, to_adjustments->scale_y);
    gdk_cairo_set_source_pixbuf(cr->cobj(), to_image->source->gobj(), to_adjustments->x, to_adjustments->y);
    cr->paint_with_alpha(fade);
    cr->restore();
    if (!from_image)
    {
        return false;
    }
    auto from_adjustments = from_image->generate_adjustments(width, height);
    if (from_adjustments == nullptr)
    {
        return false;
    }
    cr->save();
    cr->scale(from_adjustments->scale_x, from_adjustments->scale_y);
    gdk_cairo_set_source_pixbuf(cr->cobj(), from_image->source->gobj(), from_adjustments->x, from_adjustments->y);
    cr->paint_with_alpha(1.0 - fade);
    cr->restore();
    return false;
}

BackgroundDrawingArea::BackgroundDrawingArea()
{
    set_draw_func([=] (const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) { this->do_draw(cr,width,height) ;});
}

Glib::RefPtr<BackgroundImage> WayfireBackground::create_from_file_safe(std::string path)
{
    Glib::RefPtr<BackgroundImage> image = Glib::RefPtr<BackgroundImage>(new BackgroundImage());
    image->fill_type = (std::string)background_fill_mode;

    try {
        image->source = Gdk::Pixbuf::create_from_file(path);
    } catch(...)
    {
        return nullptr;
    }
    if (image->source == nullptr){
        return nullptr;
    }


    return image;
}

bool WayfireBackground::change_background()
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;
    std::string path;

    auto next_image = load_next_background();
    if (!next_image)
    {
        return false;
    }
    std::cout << "Loaded " << path << std::endl;
    drawing_area.show_image(next_image);
    return true;
}

bool WayfireBackground::load_images_from_dir(std::string path)
{
    wordexp_t exp;

    /* Expand path */
    if (wordexp(path.c_str(), &exp, 0))
    {
        return false;
    }

    if (!exp.we_wordc)
    {
        wordfree(&exp);
        return false;
    }

    auto dir = opendir(exp.we_wordv[0]);
    if (!dir)
    {
        wordfree(&exp);
        return false;
    }

    /* Iterate over all files in the directory */
    dirent *file;
    while ((file = readdir(dir)) != 0)
    {
        /* Skip hidden files and folders */
        if (file->d_name[0] == '.')
        {
            continue;
        }

        auto fullpath = std::string(exp.we_wordv[0]) + "/" + file->d_name;

        struct stat next;
        if (stat(fullpath.c_str(), &next) == 0)
        {
            if (S_ISDIR(next.st_mode))
            {
                /* Recursive search */
                load_images_from_dir(fullpath);
            } else
            {
                images.push_back(fullpath);
            }
        }
    }

    wordfree(&exp);

    if (background_randomize && images.size())
    {
        std::random_device random_device;
        std::mt19937 random_gen(random_device());
        std::shuffle(images.begin(), images.end(), random_gen);
    }

    return true;
}

Glib::RefPtr<BackgroundImage> WayfireBackground::load_next_background()
{
    Glib::RefPtr<BackgroundImage> image;
    while (!image)
    {
        if (!images.size())
        {
            std::cerr << "Failed to load background images from " <<
                    (std::string)background_image << std::endl;
            //window.remove();
            return nullptr;
        }

        current_background = (current_background + 1) % images.size();

        auto path = images[current_background];
        image = create_from_file_safe(path);

        if (!image)
        {
            images.erase(images.begin() + current_background);
        } else
        {
            std::cout << "Picked background "<< path << std::endl;
        }
    }

    return image;
}

void WayfireBackground::reset_background()
{
    images.clear();
    current_background = 0;
    change_bg_conn.disconnect();
}

void WayfireBackground::set_background()
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;

    reset_background();

    std::string path = background_image;
    try {
        if (load_images_from_dir(path) && images.size())
        {
            auto image = load_next_background();
            if (!image)
            {
                throw std::exception();
            }
            drawing_area.show_image(image);


        } else
        {
            auto image = create_from_file_safe(path);
            if (!image)
            {
                throw std::exception();
            }
            drawing_area.show_image(image);

        }
    } catch (...)
    {
        std::cerr << "Failed to load background image(s) " << path << std::endl;
    }

    reset_cycle_timeout();

    if (inhibited && output->output)
    {
        zwf_output_v2_inhibit_output_done(output->output);
        inhibited = false;
    }
}

void WayfireBackground::reset_cycle_timeout()
{
    int cycle_timeout = background_cycle_timeout * 1000;
    change_bg_conn.disconnect();
    if (images.size())
    {
        change_bg_conn = Glib::signal_timeout().connect(sigc::mem_fun(
            *this, &WayfireBackground::change_background), cycle_timeout);
    }
}

void WayfireBackground::setup_window()
{
    window.set_decorated(false);

    gtk_layer_init_for_window(window.gobj());
    gtk_layer_set_layer(window.gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_monitor(window.gobj(), this->output->monitor->gobj());

    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
    gtk_layer_set_keyboard_mode(window.gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    gtk_layer_set_exclusive_zone(window.gobj(), -1);
    window.set_child(drawing_area);

    auto reset_background = [=] () { set_background(); };
    auto reset_cycle = [=] () { reset_cycle_timeout(); };
    background_image.set_callback(reset_background);
    background_randomize.set_callback(reset_background);
    background_fill_mode.set_callback(reset_background);
    background_cycle_timeout.set_callback(reset_cycle);

    window.present();

    set_background();

}

WayfireBackground::WayfireBackground(WayfireShellApp *app, WayfireOutput *output)
{
    this->app    = app;
    this->output = output;

    if (output->output)
    {
        this->inhibited = true;
        zwf_output_v2_inhibit_output(output->output);
    }

    setup_window();
}

WayfireBackground::~WayfireBackground()
{
    reset_background();
}

class WayfireBackgroundApp : public WayfireShellApp
{
    std::map<WayfireOutput*, std::unique_ptr<WayfireBackground>> backgrounds;

  public:
    using WayfireShellApp::WayfireShellApp;
    static void create(int argc, char **argv)
    {
        WayfireShellApp::instance =
            std::make_unique<WayfireBackgroundApp>(argc, argv);
        g_unix_signal_add(SIGUSR1, sigusr1_handler, (void*)instance.get());
        instance->run();
    }

    void handle_new_output(WayfireOutput *output) override
    {
        backgrounds[output] = std::unique_ptr<WayfireBackground>(
            new WayfireBackground(this, output));
    }

    void handle_output_removed(WayfireOutput *output) override
    {
        backgrounds.erase(output);
    }

    static gboolean sigusr1_handler(void *instance)
    {
        for (const auto& [_, bg] : ((WayfireBackgroundApp*)instance)->backgrounds)
        {
            bg->change_background();
        }

        return TRUE;
    }
};

int main(int argc, char **argv)
{
    WayfireBackgroundApp::create(argc, argv);
    return 0;
}
