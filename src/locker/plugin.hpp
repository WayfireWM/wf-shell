#pragma once
#include "wf-option-wrap.hpp"
#include "lockergrid.hpp"
#define DEFAULT_PANEL_HEIGHT "48"
#define DEFAULT_ICON_SIZE 32

#define PANEL_POSITION_BOTTOM "bottom"
#define PANEL_POSITION_TOP "top"

/* Differs to panel widgets.
 *  One instance of each plugin exists and it must account for
 *  multiple widgets in multiple windows. */
class WayfireLockerPlugin
{
  public:
    WayfireLockerPlugin(std::string prefix): 
      enable(WfOption<bool>{prefix+"_enable"}), 
      always(WfOption<bool>{prefix+"_always"}),
      position(WfOption<std::string>{prefix+"_position"})
    {};
    WfOption<bool> enable, always;
    WfOption<std::string> position;
    virtual void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) = 0;
    virtual void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) = 0;
    virtual void init() = 0; /* Called just before lockscreens shown. */
    virtual void deinit() = 0; /* Called after lockscreen unlocked. */
    virtual void lockout_changed(bool lockout){}; /* Called when too many failed logins have occured */
    virtual ~WayfireLockerPlugin() = default;
};
