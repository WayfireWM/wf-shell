#pragma once
#include <gtkmm/label.h>
#include <gtkmm/image.h>

#include "plugin.hpp"
#include "lockergrid.hpp"
#include "timedrevealer.hpp"

class WayfireLockerUserPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    Gtk::Box box;
    Gtk::Image image;
    Gtk::Label label;
    WayfireLockerUserPluginWidget(std::string image_path);
};

class WayfireLockerUserPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerUserPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;

    std::map<int, Glib::RefPtr<WayfireLockerUserPluginWidget>> widgets;

    std::string image_path = "";
};
