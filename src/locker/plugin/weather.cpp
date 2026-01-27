#include <memory>
#include <fstream>
#include <iostream>
#include <glibmm.h>
#include <gtkmm/box.h>
#include <gtkmm/image.h>

#include "lockergrid.hpp"
#include "weather.hpp"
#include "wayfire/nonstd/json.hpp"


void WayfireLockerWeatherPlugin::update_labels(std::string text)
{
    for (auto& it : weather_widgets)
    {
        ((Gtk::Label *)it.second->get_first_child())->set_markup(text);
    }

    label_contents = text;
}

void WayfireLockerWeatherPlugin::update_icons(std::string path)
{
    for (auto& it : weather_widgets)
    {
        ((Gtk::Image *)it.second->get_first_child()->get_next_sibling())->set(path);
    }

    icon_path = path;
}

void WayfireLockerWeatherPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    weather_widgets.emplace(id, std::shared_ptr<Gtk::Box>(new Gtk::Box()));

    auto weather_widget = weather_widgets[id];
    if (!shown)
    {
        weather_widget->hide();
    }
    auto label = Gtk::Label();
    label.add_css_class("weather");
    label.set_markup(label_contents);
    label.set_justify(Gtk::Justification::CENTER);
    weather_widget->append(label);

    Gtk::Image icon(icon_path);
    icon.add_css_class("weather");
    weather_widget->append(icon);

    grid->attach(*weather_widget, position);
}

void WayfireLockerWeatherPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*weather_widgets[id]);
    weather_widgets.erase(id);
}

void WayfireLockerWeatherPlugin::update_weather()
{
    std::string weather_data_dir;

    weather_data_dir = std::string(getenv("HOME")) + "/.local/share/weather/data";

    std::string file_path = weather_data_dir + "/data.json";

    std::ifstream input_file(file_path);

    if (!input_file)
    {
        std::cerr << "Error reading json file " << file_path << std::endl;
        hide();
        return;
    }

    std::stringstream buf;
    buf << input_file.rdbuf();

    wf::json_t json_data;

    auto err = wf::json_t::parse_string(buf.str(), json_data);

    if (err.has_value())
    {
        std::cerr << "Error parsing json data " << file_path << ": " << *err << std::endl;
        hide();
        return;
    }

    if (!json_data.has_member("temp") || !json_data.has_member("icon"))
    {
        std::cerr << "Unexpected weather json data in " << file_path << std::endl;
        hide();
        return;
    }

    update_labels(json_data["temp"]);
    update_icons(json_data["icon"]);

    show();
}

WayfireLockerWeatherPlugin::WayfireLockerWeatherPlugin():
  WayfireLockerPlugin("locker/weather_enable", "locker/weather_position")
{}

void WayfireLockerWeatherPlugin::init()
{
    update_weather();
    timeout = Glib::signal_timeout().connect_seconds(
        [this] ()
    {
        this->update_weather();
        return G_SOURCE_CONTINUE;
    }, 600);
}

void WayfireLockerWeatherPlugin::deinit()
{
    timeout.disconnect();
}

void WayfireLockerWeatherPlugin::hide()
{
    for (auto& it : weather_widgets)
    {
        it.second->hide();
    }

    shown = false;
}

void WayfireLockerWeatherPlugin::show()
{
    for (auto& it : weather_widgets)
    {
        it.second->show();
    }

    shown = true;
}
