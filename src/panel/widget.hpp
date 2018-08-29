#ifndef WIDGET_HPP
#define WIDGET_HPP

#include <gtkmm/hvbox.h>

class wayfire_config;
class WayfireWidget
{
    public:
        virtual void init(Gtk::HBox *container, wayfire_config *config) = 0;
        virtual ~WayfireWidget() {};
};

#endif /* end of include guard: WIDGET_HPP */
