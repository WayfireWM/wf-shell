#ifndef LOCKER_PLUGIN_HPP
#define LOCKER_PLUGIN_HPP

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
    /* If true, plugin should be instantiated, init called, and used in lock screen
     *  If false, plugin will be instantiated but never init'ed.
     *
     *  We *do not* do config reloading. Sample config on construction or init, but not live */
    virtual bool should_enable() = 0;
    virtual void add_output(int id, WayfireLockerGrid *grid) = 0;
    virtual void remove_output(int id) = 0;
    virtual void init() = 0;
    virtual ~WayfireLockerPlugin() = default;
};

#endif
