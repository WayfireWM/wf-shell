#include "weather.hpp"

void WayfireWeather::init(Gtk::Box *container)
{
    weather.add_css_class("weather");
    container->append(weather);
}

WayfireWeather::~WayfireWeather() = default;
