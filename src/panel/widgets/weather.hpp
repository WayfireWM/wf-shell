#pragma once

#include "../widget.hpp"
#include "widget-utils/weather.hpp"

class WayfireWeather : public WayfireWidget
{
    ShellWeather weather;

  public:
    void init(Gtk::Box *container) override;
    ~WayfireWeather();
};
