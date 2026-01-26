#pragma once

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <gtkmm/label.h>
#include <gtkmm/image.h>

class WayfireWeather : public WayfireWidget
{
    Gtk::Label label;
    Gtk::Image icon;
    Gtk::Box box;

    sigc::connection timeout;

  public:
    void init(Gtk::Box *container) override;
    bool update_weather();
    void update_label(std::string);
    void update_icon(std::string);
    ~WayfireWeather();
};
