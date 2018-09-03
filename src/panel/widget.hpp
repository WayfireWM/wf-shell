#ifndef WIDGET_HPP
#define WIDGET_HPP

#include <gtkmm/hvbox.h>

#define DEFAULT_PANEL_HEIGHT "48"

class wayfire_config;
class WayfireWidget
{
    public:
        virtual void init(Gtk::HBox *container, wayfire_config *config) = 0;
        virtual ~WayfireWidget() {};
};

#endif /* end of include guard: WIDGET_HPP */
