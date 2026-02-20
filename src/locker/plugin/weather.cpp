#include <memory>
#include <fstream>
#include <iostream>
#include <glibmm.h>

#include "gtkmm/enums.h"
#include "lockergrid.hpp"
#include "weather.hpp"
#include "timedrevealer.hpp"
#include "wayfire/nonstd/json.hpp"


void WayfireLockerWeatherPlugin::update_labels(std::string text)
{
    for (auto& it : weather_widgets)
    {
        it.second->label.set_markup(text);
    }

    label_contents = text;
}

void WayfireLockerWeatherPlugin::update_icons(std::string path)
{
    for (auto& it : weather_widgets)
    {
        it.second->image.set(path);
    }

    icon_path = path;
}

WayfireLockerWeatherPluginWidget::WayfireLockerWeatherPluginWidget(std::string contents,
    std::string icon_path) :
    WayfireLockerTimedRevealer("locker/weather_always")
{
    set_child(box);
    label.add_css_class("weather");
    label.set_markup(contents);
    label.set_justify(Gtk::Justification::CENTER);
    box.prepend(label);

    image.add_css_class("weather");
    image.set(icon_path);
    box.append(image);
}

void WayfireLockerWeatherPlugin::add_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    weather_widgets.emplace(id, new WayfireLockerWeatherPluginWidget(label_contents, icon_path));

    auto weather_widget = weather_widgets[id];
    if (!shown)
    {
        weather_widget->hide();
    }

    grid->attach(*weather_widget, position);
}

void WayfireLockerWeatherPlugin::remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid)
{
    grid->remove(*weather_widgets[id]);
    weather_widgets.erase(id);
}

bool WayfireWeather::handle_inotify_event(Glib::IOCondition cond)
{
	if (cond == Glib::IOCondition::IO_HUP)
	{
        return false;
    }

	char buf[1024 * sizeof(inotify_event)];
    read(inotify_fd, buf, sizeof(buf));

    update_weather();

    return true;
}

void WayfireLockerWeatherPlugin::update_weather()
{
    std::ifstream input_file(weather_data_path);

    if (!input_file)
    {
        std::cerr << "Error reading json file " << weather_data_path << std::endl;
        hide();
        return;
    }

    std::stringstream buf;
    buf << input_file.rdbuf();

    wf::json_t json_data;

    auto err = wf::json_t::parse_string(buf.str(), json_data);

    if (err.has_value())
    {
        std::cerr << "Error parsing json data " << weather_data_path << ": " << *err << std::endl;
        hide();
        return;
    }

    if (!json_data.has_member("temp") || !json_data.has_member("icon"))
    {
        std::cerr << "Unexpected weather json data in " << weather_data_path << std::endl;
        hide();
        return;
    }

    update_labels(json_data["temp"]);
    update_icons(json_data["icon"]);

    show();
}

WayfireLockerWeatherPlugin::WayfireLockerWeatherPlugin() :
    WayfireLockerPlugin("locker/weather")
{}

void WayfireLockerWeatherPlugin::init()
{
    weather_data_path = std::string(getenv("HOME")) + "/.local/share/owf/data/data.json";

    inotify_fd = inotify_init();

    inotify_add_watch(inotify_fd,
        weather_data_path.c_str(),
        IN_CLOSE_WRITE);

    inotify_connection = Glib::signal_io().connect(
        sigc::mem_fun(*this, &WayfireWeather::handle_inotify_event),
        inotify_fd, Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);

    update_weather();
}

void WayfireLockerWeatherPlugin::deinit()
{
    inotify_connection.disconnect();
    close(inotify_fd);
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
