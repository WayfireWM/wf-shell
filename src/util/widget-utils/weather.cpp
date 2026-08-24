#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

#include <glibmm.h>

#include "wayfire/nonstd/json.hpp"
#include "weather.hpp"


ShellWeather::ShellWeather()
{
    set_orientation(Gtk::Orientation::HORIZONTAL);

    label.set_justify(Gtk::Justification::CENTER);
    prepend(label);
    append(icon);

    weather_data_path = std::string(getenv("HOME")) + "/.local/share/owf/data/data.json";

    inotify_fd = inotify_init();
    if (inotify_fd < 0)
    {
        std::cerr << "Error creating inotify instance" << std::endl;
        notify_visibility(false);
        return;
    }

    if (inotify_add_watch(inotify_fd, weather_data_path.c_str(), IN_CLOSE_WRITE) < 0)
    {
        std::cerr << "Error watching json file " << weather_data_path << std::endl;
        notify_visibility(false);
        return;
    }

    inotify_connection = Glib::signal_io().connect(
        sigc::mem_fun(*this, &ShellWeather::handle_inotify_event),
        inotify_fd, Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);

    update_weather();
}

ShellWeather::~ShellWeather()
{
    inotify_connection.disconnect();
    if (inotify_fd >= 0)
    {
        close(inotify_fd);
    }
}

void ShellWeather::set_visibility_callback(std::function<void(bool)> callback)
{
    visibility_callback = std::move(callback);
}

void ShellWeather::notify_visibility(bool visible)
{
    if (visibility_callback)
    {
        visibility_callback(visible);
    }
}

bool ShellWeather::handle_inotify_event(Glib::IOCondition cond)
{
    if (cond == Glib::IOCondition::IO_HUP)
    {
        notify_visibility(false);
        return false;
    }

    char buf[1024 * sizeof(inotify_event)];
    read(inotify_fd, buf, sizeof(buf));

    update_weather();
    return true;
}

void ShellWeather::update_weather()
{
    std::ifstream input_file(weather_data_path);
    if (!input_file)
    {
        std::cerr << "Error reading json file " << weather_data_path << std::endl;
        notify_visibility(false);
        return;
    }

    std::stringstream buffer;
    buffer << input_file.rdbuf();

    wf::json_t json_data;
    auto err = wf::json_t::parse_string(buffer.str(), json_data);
    if (err.has_value())
    {
        std::cerr << "Error parsing json data " << weather_data_path << ": " << *err << std::endl;
        notify_visibility(false);
        return;
    }

    if (!json_data.has_member("temp") || !json_data.has_member("icon"))
    {
        std::cerr << "Unexpected weather json data in " << weather_data_path << std::endl;
        notify_visibility(false);
        return;
    }

    label.set_text(json_data["temp"].as_string());
    icon.set(json_data["icon"].as_string());
    notify_visibility(true);
}
