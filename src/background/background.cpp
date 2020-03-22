#include <wordexp.h>
#include <glibmm/main.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <gtkmm/image.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/general.h>
#include <gdk/gdkwayland.h>

#include <random>
#include <algorithm>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <gtk-layer-shell.h>

#include "background.hpp"


void BackgroundDrawingArea::show_image(Glib::RefPtr<Gdk::Pixbuf> image,
    double offset_x, double offset_y)
{
    if (!image)
    {
        to_image.source.clear();
        from_image.source.clear();
        return;
    }

    from_image = to_image;
    to_image.source = Gdk::Cairo::create_surface_from_pixbuf(image,
        this->get_scale_factor());

    to_image.x = offset_x / this->get_scale_factor();
    to_image.y = offset_y / this->get_scale_factor();
    fade.animate(from_image.source ? 0.0 : 1.0, 1.0);

    Glib::signal_idle().connect_once([=] () {
        this->queue_draw();
    });
}

bool BackgroundDrawingArea::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (!to_image.source)
        return false;

    if (fade.running())
        queue_draw();

    cr->set_source(to_image.source, to_image.x, to_image.y);
    cr->paint_with_alpha(fade);
    if (!from_image.source)
        return false;

    cr->set_source(from_image.source, from_image.x, from_image.y);
    cr->paint_with_alpha(1.0 - fade);
    return false;
}

BackgroundDrawingArea::BackgroundDrawingArea()
{
    fade.animate(0, 0);
}

Glib::RefPtr<Gdk::Pixbuf>
WayfireBackground::create_from_file_safe(std::string path)
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;
    int width = window.get_allocated_width() * scale;
    int height = window.get_allocated_height() * scale;

    try {
        pbuf =
            Gdk::Pixbuf::create_from_file(path, width, height,
                background_preserve_aspect);
    } catch (...) {
        return Glib::RefPtr<Gdk::Pixbuf>();
    }

    if (background_preserve_aspect)
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

bool WayfireBackground::change_background(int timer)
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;
    std::string path;

    if (!load_next_background(pbuf, path))
        return false;

    std::cout << "Loaded " << path << std::endl;
    drawing_area.show_image(pbuf, offset_x, offset_y);
    return true;
}

bool WayfireBackground::load_images_from_dir(std::string path)
{
    wordexp_t exp;

    /* Expand path */
    wordexp(path.c_str(), &exp, 0);
    if (!exp.we_wordv)
        return false;

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
            if (S_ISDIR(next.st_mode)) {
                /* Recursive search */
                load_images_from_dir(fullpath);
            } else {
                images.push_back(fullpath);
            }
        }
    }

    if (background_randomize && images.size())
    {
        std::random_device random_device;
        std::mt19937 random_gen(random_device());
        std::shuffle(images.begin(), images.end(), random_gen);
    }

    return true;
}

bool WayfireBackground::load_next_background(Glib::RefPtr<Gdk::Pixbuf> &pbuf,
    std::string &path)
{
    while (!pbuf)
    {
        if (!images.size())
        {
            std::cerr << "Failed to load background images from "
                << (std::string)background_image << std::endl;
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

void WayfireBackground::reset_background()
{
    images.clear();
    current_background = 0;
    change_bg_conn.disconnect();
    scale = window.get_scale_factor();
}

void WayfireBackground::set_background()
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;

    reset_background();

    std::string path = background_image;
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
    } catch (...)
    {
        std::cerr << "Failed to load background image(s) " << path << std::endl;
    }

    reset_cycle_timeout();
    drawing_area.show_image(pbuf, offset_x, offset_y);

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
        change_bg_conn = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(
            this, &WayfireBackground::change_background), 0), cycle_timeout);
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

    gtk_layer_set_exclusive_zone(window.gobj(), -1);
    window.add(drawing_area);
    window.show_all();

    auto reset_background = [=] () { set_background(); };
    auto reset_cycle = [=] () { reset_cycle_timeout(); };
    background_image.set_callback(reset_background);
    background_randomize.set_callback(reset_background);
    background_preserve_aspect.set_callback(reset_background);
    background_cycle_timeout.set_callback(reset_cycle);

    window.property_scale_factor().signal_changed().connect(
        sigc::mem_fun(this, &WayfireBackground::set_background));
}

WayfireBackground::WayfireBackground(WayfireShellApp *app, WayfireOutput *output)
{
    this->app = app;
    this->output = output;

    if (output->output)
    {
        this->inhibited = true;
        zwf_output_v2_inhibit_output(output->output);
    }

    setup_window();

    this->window.signal_size_allocate().connect_notify(
        [this, width = 0, height = 0] (Gtk::Allocation& alloc) mutable
        {
            if (alloc.get_width() != width || alloc.get_height() != height)
            {
                this->set_background();
                width = alloc.get_width();
                height = alloc.get_height();
            }
        });
}

class WayfireBackgroundApp : public WayfireShellApp
{
    std::map<WayfireOutput*, std::unique_ptr<WayfireBackground> > backgrounds;

  public:
    using WayfireShellApp::WayfireShellApp;
    static void create(int argc, char **argv)
    {
        WayfireShellApp::instance =
            std::make_unique<WayfireBackgroundApp> (argc, argv);
        instance->run();
    }

    void handle_new_output(WayfireOutput *output) override
    {
        backgrounds[output] = std::unique_ptr<WayfireBackground> (
            new WayfireBackground(this, output));
    }

    void handle_output_removed(WayfireOutput *output) override
    {
        backgrounds.erase(output);
    }
};

int main(int argc, char **argv)
{
    WayfireBackgroundApp::create(argc, argv);
    return 0;
}
