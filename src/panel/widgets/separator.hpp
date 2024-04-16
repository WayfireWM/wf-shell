#ifndef WIDGET_SEPARATOR_HPP
#define WIDGET_SEPARATORG_HPP

#include "../widget.hpp"
#include <gtkmm/eventbox.h>

class WayfireSeparator : public WayfireWidget
{
    Gtk::EventBox box;

  public:
    WayfireSeparator(int pixels);

    virtual void init(Gtk::HBox *container);
    virtual ~WayfireSeparator()
    {}
};


#endif /* end of include guard: WIDGET_SEPARATOR_HPP */
