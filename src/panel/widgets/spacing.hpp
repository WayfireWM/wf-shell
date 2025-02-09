#ifndef WIDGET_SPACING_HPP
#define WIDGET_SPACING_HPP

#include "../widget.hpp"

class WayfireSpacing : public WayfireWidget
{
    Gtk::Box box;

  public:
    WayfireSpacing(int pixels);

    virtual void init(Gtk::Box *container);
    virtual ~WayfireSpacing()
    {}
};


#endif /* end of include guard: WIDGET_SPACING_HPP */
