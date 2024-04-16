#ifndef WIDGET_SPACING_HPP
#define WIDGET_SPACING_HPP

#include "../widget.hpp"
#include <gtkmm/eventbox.h>

class WayfireSpacing : public WayfireWidget
{
    Gtk::EventBox box;
    WfOption<bool> visible{"panel/spacing_visible"};

    void update_visible();


  public:
    WayfireSpacing(int pixels);

    virtual void init(Gtk::HBox *container);
    virtual ~WayfireSpacing()
    {}
};


#endif /* end of include guard: WIDGET_SPACING_HPP */
