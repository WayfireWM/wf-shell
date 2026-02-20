#include <sys/inotify.h>
#include <iostream>
#include <fstream>
#include <glibmm.h>
#include "weather.hpp"
#include "wayfire/nonstd/json.hpp"

void WayfireWeather::init(Gtk::Box *container)
{
    label.set_justify(Gtk::Justification::CENTER);
    box.get_style_context()->add_class("weather");
    box.prepend(label);
    box.append(icon);
    container->append(box);

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

void WayfireWeather::update_weather()
{

    std::ifstream input_file(weather_data_path);

    if (!input_file)
    {
        std::cerr << "Error reading json file " << weather_data_path << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << input_file.rdbuf();

    wf::json_t json_data;

    auto err = wf::json_t::parse_string(buffer.str(), json_data);

    if (err.has_value())
    {
        std::cerr << "Error parsing json data " << weather_data_path << ": " << *err << std::endl;
        return;
    }

    if (!json_data.has_member("temp") || !json_data.has_member("icon"))
    {
        std::cerr << "Unexpected weather json data in " << weather_data_path << std::endl;
        return;
    }

    update_label(json_data["temp"]);
    update_icon(json_data["icon"]);
}

void WayfireWeather::update_label(std::string temperature)
{
    label.set_text(temperature);
}

void WayfireWeather::update_icon(std::string path)
{
    icon.set(path);
}

WayfireWeather::~WayfireWeather()
{
    inotify_connection.disconnect();
    close(inotify_fd);
}
