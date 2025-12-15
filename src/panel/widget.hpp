#ifndef WIDGET_HPP
#define WIDGET_HPP

#include <gtkmm/box.h>
#include <wf-option-wrap.hpp>
#include <wayfire/config/types.hpp>

#define DEFAULT_PANEL_HEIGHT "48"
#define DEFAULT_ICON_SIZE 32

#define PANEL_POSITION_TOP "top"
#define PANEL_POSITION_BOTTOM "bottom"
#define PANEL_POSITION_LEFT "left"
#define PANEL_POSITION_RIGHT "right"

#define PANEL_ORIENTATION_HORIZONTAL "horizontal"
#define PANEL_ORIENTATION_LEFT "left"
#define PANEL_ORIENTATION_RIGHT "right"

class wayfire_config;
class WayfireWidget
{
  public:
    std::string widget_name; // for WayfirePanel use, widgets shouldn't change it

    virtual void init(Gtk::Box *container) = 0;

    struct config{
      inline static std::string panel_position = "top";
      inline static bool is_horizontal = true; // with how many times it would be computed have a shorthand for it. not even sure there’s a difference after optimisitions tbh
      inline static std::string panel_orientation = "horizontal";
    };

    virtual void handle_config_reload()
    {}
    virtual ~WayfireWidget()
    {}
};

#endif /* end of include guard: WIDGET_HPP */
