#pragma once
#include <gtkmm/label.h>
#include <unordered_map>

#include "plugin.hpp"
#include "timedrevealer.hpp"
#include "wf-option-wrap.hpp"
#include "lockergrid.hpp"

class WayfireLockerClockPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    Gtk::Label label;
    WayfireLockerClockPluginWidget(std::string contents);
};

class WayfireLockerClockPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerClockPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;

    WfOption<std::string> format{"locker/clock_format"};

    sigc::connection timeout;
    void update_labels(std::string text);
    void update_time();

    std::unordered_map<int, std::shared_ptr<WayfireLockerClockPluginWidget>> widgets;
    std::string label_contents = "";
};
