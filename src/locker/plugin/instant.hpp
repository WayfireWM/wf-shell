#pragma once
#include <gtkmm/button.h>

#include "plugin.hpp"
#include "lockergrid.hpp"
#include "timedrevealer.hpp"

class WayfireLockerInstantPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    Gtk::Button button;
    WayfireLockerInstantPluginWidget();
};

class WayfireLockerInstantPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerInstantPlugin();
    void add_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(std::string id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;

    std::unordered_map<std::string, std::shared_ptr<WayfireLockerInstantPluginWidget>> widgets;
};
