#pragma once

#include <sys/inotify.h>
#include <functional>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <glibmm/iochannel.h>
#include <sigc++/connection.h>

class ShellWeather : public Gtk::Box
{
  public:
    ShellWeather();
    ~ShellWeather() override;

    void set_visibility_callback(std::function<void(bool)> callback);

  private:
    Gtk::Label label;
    Gtk::Image icon;

    int inotify_fd = -1;
    sigc::connection inotify_connection;
    std::string weather_data_path;
    std::function<void(bool)> visibility_callback;

    bool handle_inotify_event(Glib::IOCondition cond);
    void update_weather();
    void notify_visibility(bool visible);
};
