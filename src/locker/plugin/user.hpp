#pragma once
#include <gtkmm/label.h>
#include <gtkmm/image.h>

#include "plugin.hpp"
#include "lockergrid.hpp"

class WayfireLockerUserPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerUserPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;

    std::map<int, Glib::RefPtr<Gtk::Label>> labels;
    std::map<int, Glib::RefPtr<Gtk::Image>> images;
    std::map<int, Glib::RefPtr<Gtk::Box>> boxes;

    std::string image_path = "";
};
