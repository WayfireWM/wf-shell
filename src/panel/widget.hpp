#ifndef WIDGET_HPP
#define WIDGET_HPP

#include <gtkmm/container.h>

class WayfireWidget
{
    public:
        virtual void init(Gtk::Container *container) = 0;
        virtual int get_width() = 0;

        virtual ~WayfireWidget() {};
};

#endif /* end of include guard: WIDGET_HPP */
