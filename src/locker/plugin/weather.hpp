#pragma once
#include <gtkmm/label.h>
#include <unordered_map>

#include "plugin.hpp"
#include "wf-option-wrap.hpp"
#include "lockergrid.hpp"

class WayfireLockerWeatherPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerWeatherPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;

    void update_labels(std::string text);
    void update_icons(std::string path);
    void update_weather();

    std::unordered_map<int, std::shared_ptr<Gtk::Box>> weather_widgets;
    std::vector<std::shared_ptr<Gtk::Label>> labels;
    std::vector<std::shared_ptr<Gtk::Image>> icons;
    std::string label_contents = "";
    std::string icon_path = "";
};
