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
    WayfireLockerPlugin(std::string option_name, std::string position_name): 
      enable(WfOption<bool>{option_name}), 
      position(WfOption<std::string>{position_name})
    {};
    WfOption<bool> enable;
    WfOption<std::string> position;
    virtual void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) = 0;
    virtual void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) = 0;
    virtual void init() = 0;
    virtual void deinit() = 0;
    virtual ~WayfireLockerPlugin() = default;
};
