#include <iostream>

#include "lockergrid.hpp"
#include "user.hpp"

WayfireLockerUserPlugin::WayfireLockerUserPlugin() :
    enable(WfOption<bool>{"locker/user_enable"})
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
            std::cout << home_path_file << " Selected" << std::endl;
            image_path = home_path_file;
            return;
        }
    }

    std::cout << "No user image .face... no image in lockscreen" << std::endl;
}

void WayfireLockerUserPlugin::add_output(int id, WayfireLockerGrid *grid)
{
    labels.emplace(id, Glib::RefPtr<Gtk::Label>(new Gtk::Label()));
    images.emplace(id, Glib::RefPtr<Gtk::Image>(new Gtk::Image()));
    boxes.emplace(id, Glib::RefPtr<Gtk::Box>(new Gtk::Box));

    auto label = labels[id];
    auto image = images[id];
    auto box   = boxes[id];

    box->add_css_class("user");
    box->set_orientation(Gtk::Orientation::VERTICAL);
    image->set_halign(Gtk::Align::CENTER);
    image->set_valign(Gtk::Align::END);
    label->set_halign(Gtk::Align::CENTER);
    label->set_valign(Gtk::Align::START);
    label->set_justify(Gtk::Justification::CENTER);

    std::string username = getlogin();

    label->set_label(username);
    if (image_path == "")
    {
        image->hide();
    } else
    {
        image->set(image_path);
    }

    box->append(*image);
    box->append(*label);
    grid->attach(*box, WfOption<std::string>{"locker/user_position"});
}

void WayfireLockerUserPlugin::remove_output(int id)
{
    labels.erase(id);
    images.erase(id);
    boxes.erase(id);
}

bool WayfireLockerUserPlugin::should_enable()
{
    return enable;
}
