#ifndef WIDGET_HPP
#define WIDGET_HPP

#include <gtkmm/hvbox.h>

#define DEFAULT_PANEL_HEIGHT "48"

class wayfire_config;
class WayfireWidget
{
    public:
        std::string widget_name; // for WayfirePanel use, widgets shouldn't change it

        virtual void init(Gtk::HBox *container, wayfire_config *config) = 0;
        virtual void focus_lost() {} // used to hide popovers
        virtual void handle_config_reload(wayfire_config *config) {}
        virtual ~WayfireWidget() {};
};

#endif /* end of include guard: WIDGET_HPP */
