#include <fcntl.h>
#include <wordexp.h>
#include <glibmm/main.h>
#include <gtkmm.h>
#include <gdkmm.h>
#include <gdk/wayland/gdkwayland.h>
#include <random>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <sys/stat.h>

#include <gtk-utils.hpp>
#include <gtk4-layer-shell.h>
#include <glib-unix.h>

#include "background.hpp"
#include "background-gl.hpp"

void WayfireBackground::setup_window()
{
    gl_area = Glib::RefPtr<BackgroundGLArea>(new BackgroundGLArea());
    set_decorated(false);

    gtk_layer_init_for_window(gobj());
    gtk_layer_set_layer(gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_monitor(gobj(), this->output->monitor->gobj());

    gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
    gtk_layer_set_anchor(gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
    gtk_layer_set_keyboard_mode(gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    gtk_layer_set_exclusive_zone(gobj(), -1);

    set_child(*gl_area);
    present();
}

WayfireBackground::WayfireBackground(WayfireOutput *output)
{
    this->output = output;

    if (output->output && inhibit_start)
    {
        this->inhibited = true;
        zwf_output_v2_inhibit_output(output->output);
    }

    setup_window();
}

WayfireBackground::~WayfireBackground()
{}

int main(int argc, char **argv)
{
    WayfireBackgroundApp::create(argc, argv);
    return 0;
}

void WayfireBackgroundApp::create(int argc, char **argv)
{
    if (instance)
    {
        throw std::logic_error("Running WayfireBackgroundApp twice!");
    }

    instance = std::unique_ptr<WayfireShellApp>(new WayfireBackgroundApp{});
    instance->init_app();
    g_unix_signal_add(SIGUSR1, sigusr1_handler, (void*)instance.get());
    instance->run(argc, argv);
}

void WayfireBackgroundApp::on_activate()
{
    WayfireShellApp::on_activate();

    prep_cache();
    change_background();
}

void WayfireBackgroundApp::prep_cache()
{
    char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir)
    {
        cache_file = std::string(xdg_runtime_dir) + "/wf-background.cache";
        std::cout << "Using cache file " << this->cache_file << std::endl;
    } else
    {
        std::cout << "Not writing to cache file " << std::endl;
    }
}

void WayfireBackgroundApp::handle_new_output(WayfireOutput *output)
{
    backgrounds[output] = std::unique_ptr<WayfireBackground>(
        new WayfireBackground(output));
}

void WayfireBackgroundApp::handle_output_removed(WayfireOutput *output)
{
    backgrounds.erase(output);
}

std::string WayfireBackgroundApp::get_application_name()
{
    return "org.wayfire.background";
}

std::vector<std::string> WayfireBackgroundApp::get_background_list(std::string path)
{
    wordexp_t exp;
    std::cout << "Looking in " << path << std::endl;
    /* Expand path */
    if (wordexp(path.c_str(), &exp, 0))
    {
        std::cout << "Error getting list of images" << std::endl;
        exit(0);
    }

    if (!exp.we_wordc)
    {
        std::cout << "Error getting list of images" << std::endl;
        exit(0);
    }

    std::vector<std::string> images;
    struct stat s;
    if (stat(path.c_str(), &s)==0)
    {
        if (s.st_mode & S_IFREG || s.st_mode & S_IFLNK)
        {
            wordfree(&exp);
            images.push_back(path);
            return images;
        }
    }

    auto dir = opendir(exp.we_wordv[0]);
    if (!dir)
    {
        std::cout << "Error getting list of images" << std::endl;
        exit(0);
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
                auto list = get_background_list(fullpath);
                images.insert(std::end(images), std::begin(list), std::end(list));
            } else
            {
                images.push_back(fullpath);
            }
        }
    }

    wordfree(&exp);

    bool background_randomize = WfOption<bool>{"background/randomize"};
    if (background_randomize && images.size())
    {
        std::random_device random_device;
        std::mt19937 random_gen(random_device());
        std::shuffle(images.begin(), images.end(), random_gen);
    }

    return images;
}

void WayfireBackgroundApp::change_background()
{
    std::string background_path = WfOption<std::string>{"background/image"};
    auto list = get_background_list(background_path);
    auto idx  = find(list.begin(), list.end(), current_background) - list.begin();
    for (auto var : list)
    {
        std::cout << var << std::endl;
    }

    std::cout << current_background << "  " << idx << std::endl;
    idx = (idx + 1) % list.size();
    // gl_area->show_image(list[idx]);
    for (auto & it : backgrounds)
    {
        it.second->gl_area->show_image(list[idx]);
    }

    write_cache(list[idx]);
    reset_cycle_timeout();
    current_background = list[idx];
}

gboolean WayfireBackgroundApp::sigusr1_handler(void *instance)
{
    ((WayfireBackgroundApp*)instance)->change_background();
    return TRUE;
}

void WayfireBackgroundApp::write_cache(std::string path)
{
    std::cout << "Writing path to cache " << path << std::endl;
    if (cache_file.length() > 1)
    {
        std::ofstream out(cache_file);
        out << path;
        out.close();
    }
}

void WayfireBackgroundApp::reset_cycle_timeout()
{
    int background_cycle = 1000 * WfOption<int>{"background/cycle_timeout"};
    change_bg_conn.disconnect();
    change_bg_conn = Glib::signal_timeout().connect(
        [this] ()
    {
        change_background();
        return G_SOURCE_REMOVE;
    }, background_cycle);
}
