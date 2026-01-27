#include <iostream>

#include "lockergrid.hpp"
#include "plugin.hpp"
#include "timedrevealer.hpp"
#include "user.hpp"

WayfireLockerUserPlugin::WayfireLockerUserPlugin():
    WayfireLockerPlugin("locker/user")
{}

void WayfireLockerUserPlugin::init()
{
    char *home = getenv("HOME");
    if (home == NULL)
    {
        std::cout << "No user home, skipping finding image" << std::endl;
        return;
    }

    std::string home_path = home;

    std::vector<std::string> paths = {
        ".face",
        ".face.png",
        ".face.jpg",
        ".face.jpeg",
        ".face.svg",
        ".face.icon",
    };

    for (auto path : paths)
    {
        auto home_path_file = home_path + "/" + path;
        struct stat sb;
        if ((stat(home_path_file.c_str(), &sb) == 0) && !(sb.st_mode & S_IFDIR))
        {
            std::cout << "Chosen image "<<home_path_file << std::endl;
            image_path = home_path_file;
            return;
        }
    }

    std::cout << "No user image .face... no image in lockscreen" << std::endl;
}

void WayfireLockerUserPlugin::deinit()
{
    image_path="";
}

WayfireLockerUserPluginWidget::WayfireLockerUserPluginWidget(std::string image_path):
    WayfireLockerTimedRevealer("locker/user_always")
{
    set_child(box);
    box.add_css_class("user");
    box.set_orientation(Gtk::Orientation::VERTICAL);
    image.set_halign(Gtk::Align::CENTER);
    image.set_valign(Gtk::Align::END);
    label.set_halign(Gtk::Align::CENTER);
    label.set_valign(Gtk::Align::START);
    label.set_justify(Gtk::Justification::CENTER);
    std::string username = getlogin();
    label.set_label(username);
    if (image_path == "")
    {
        image.hide();
    } else
    {
        image.set(image_path);
    }
    box.append(image);
    box.append(label);
}

void WayfireLockerUserPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    widgets.emplace(id, new WayfireLockerUserPluginWidget(image_path));
    auto widget = widgets[id];
    grid->attach(*widget, position);
}

void WayfireLockerUserPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*widgets[id]);
    widgets.erase(id);
}