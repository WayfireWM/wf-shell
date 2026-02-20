#pragma once
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <unordered_map>

#include "plugin.hpp"
#include "timedrevealer.hpp"
#include "wf-option-wrap.hpp"
#include "lockergrid.hpp"

class WayfireLockerWeatherPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    Gtk::Box box;
    Gtk::Label label;
    Gtk::Image image;
    WayfireLockerWeatherPluginWidget(std::string contents, std::string icon_path);
};

class WayfireLockerWeatherPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerWeatherPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;


    int inotify_fd;
    sigc::connection inotify_connection;
    std::string weather_data_path;
    bool handle_inotify_event(Glib::IOCondition cond);
    void update_weather();
    void update_labels(std::string text);
    void update_icons(std::string path);

    void hide();
    void show();

    std::unordered_map<int, std::shared_ptr<WayfireLockerWeatherPluginWidget>> weather_widgets;
    std::string label_contents = "";
    std::string icon_path = "";
    bool shown;
};
