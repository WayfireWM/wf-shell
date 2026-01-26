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

    update_weather();

    timeout = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &WayfireWeather::update_weather), 600);
}

bool WayfireWeather::update_weather()
{
    std::string weather_data_dir;

    weather_data_dir = std::string(getenv("HOME")) + "/.local/share/weather/data";

    std::string file_path = weather_data_dir + "/data.json";

    std::ifstream input_file(file_path);

    if (!input_file)
    {
        std::cerr << "Error reading json file " << file_path << std::endl;
        return false;
    }

    std::stringstream buf;
    buf << input_file.rdbuf();

    wf::json_t json_data;

    auto err = wf::json_t::parse_string(buf.str(), json_data);

    if (err.has_value())
    {
        std::cerr << "Error parsing json data " << file_path << ": " << *err << std::endl;
        return false;
    }

    if (!json_data.has_member("temp") || !json_data.has_member("icon"))
    {
        std::cerr << "Unexpected weather json data in " << file_path << std::endl;
        return false;
    }

    update_label(json_data["temp"]);
    update_icon(json_data["icon"]);

    return true;
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
    timeout.disconnect();
}
