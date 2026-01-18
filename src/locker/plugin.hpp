#ifndef LOCKER_PLUGIN_HPP
#define LOCKER_PLUGIN_HPP

#include <gtkmm/grid.h>
#include <gtkmm/box.h>

#define DEFAULT_PANEL_HEIGHT "48"
#define DEFAULT_ICON_SIZE 32

#define PANEL_POSITION_BOTTOM "bottom"
#define PANEL_POSITION_TOP "top"

/* Differs to panel widgets.
   One instance of each plugin exists and it must account for 
   multiple widgets in multiple windows. */
class WayfireLockerPlugin
{
  public:
    /* If true, plugin should be instantiated, init called, and used in lock screen
       If false, plugin will be instantiated but never init'ed.
       
       We *do not* do config reloading. Sample config on construction or init, but not live */
    virtual bool should_enable() = 0;
    virtual void add_output(int id, Gtk::Grid *grid) = 0;
    virtual void remove_output(int id) = 0;
    virtual void init() = 0;
    virtual ~WayfireLockerPlugin() = default;

    /* Config string to box from grid */
    Gtk::Box* get_plugin_position(std::string pos_string, Gtk::Grid *grid){
      if (pos_string == "top-left")
      {
         return (Gtk::Box*)grid->get_child_at(0, 0);
      } else if (pos_string == "top-center")
      {
         return (Gtk::Box*)grid->get_child_at(1, 0);
      } else if (pos_string == "top-right")
      {
         return (Gtk::Box*)grid->get_child_at(2, 0);
      } else if (pos_string == "center-left")
      {
         return (Gtk::Box*)grid->get_child_at(0, 1);
      } else if (pos_string == "center-center")
      {
         return (Gtk::Box*)grid->get_child_at(1, 1);
      } else if (pos_string == "center-right")
      {
         return (Gtk::Box*)grid->get_child_at(2, 1);
      } else if (pos_string == "bottom-left")
      {
         return (Gtk::Box*)grid->get_child_at(0, 2);
      } else if (pos_string == "bottom-center")
      {
         return (Gtk::Box*)grid->get_child_at(1, 2);
      } else if (pos_string == "bottom-right")
      {
         return (Gtk::Box*)grid->get_child_at(2, 2);
      }
      return nullptr;
    }
};

#endif 