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

    int inotify_fd;
    sigc::connection inotify_connection;
    std::string weather_data_path;

  public:
    void init(Gtk::Box *container) override;
    bool handle_inotify_event(Glib::IOCondition cond);
    void update_weather();
    void update_label(std::string);
    void update_icon(std::string);
    ~WayfireWeather();
};
