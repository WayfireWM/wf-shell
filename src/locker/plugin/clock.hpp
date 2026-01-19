#ifndef LOCKER_CLOCK_PLUGIN_HPP
#define LOCKER_CLOCK_PLUGIN_HPP

#include <gtkmm/label.h>
#include <unordered_map>

#include "../plugin.hpp"
#include "../../util/wf-option-wrap.hpp"
#include "lockergrid.hpp"

class WayfireLockerClockPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerClockPlugin();
    void add_output(int id, WayfireLockerGrid *grid) override;
    void remove_output(int id) override;
    bool should_enable() override;
    void init() override;

    WfOption<bool> enable{"locker/clock_enable"};
    WfOption<std::string> format{"locker/clock_format"};

    sigc::connection timeout;
    void update_labels(std::string text);
    void update_time();

    std::unordered_map<int, std::shared_ptr<Gtk::Label>> labels;
    std::string label_contents = "";
};

#endif
