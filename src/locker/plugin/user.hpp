#pragma once
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#include <unordered_map>

#include "../plugin.hpp"
#include "../../util/wf-option-wrap.hpp"
#include "lockergrid.hpp"

class WayfireLockerUserPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerUserPlugin();
    void add_output(int id, WayfireLockerGrid *grid) override;
    void remove_output(int id) override;
    bool should_enable() override;
    void init() override;

    bool enable;

    std::unordered_map<int, Glib::RefPtr<Gtk::Label>> labels;
    std::unordered_map<int, Glib::RefPtr<Gtk::Image>> images;
    std::unordered_map<int, Glib::RefPtr<Gtk::Box>> boxes;

    std::string image_path="";
};
