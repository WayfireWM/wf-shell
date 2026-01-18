#ifndef LOCKER_INSTANT_PLUGIN_HPP
#define LOCKER_INSTANT_PLUGIN_HPP

#include <gtkmm/button.h>

#include "../plugin.hpp"
#include "../../util/wf-option-wrap.hpp"

class WayfireLockerInstantPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerInstantPlugin()
    {}
    void add_output(int id, Gtk::Grid *grid) override;
    void remove_output(int id) override;
    bool should_enable() override;
    void init() override;

    WfOption<bool> enable{"locker/instant_unlock_enable"};

    std::unordered_map<int, std::shared_ptr<Gtk::Button>> buttons;
};

#endif
