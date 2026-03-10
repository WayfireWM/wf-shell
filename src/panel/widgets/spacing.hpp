#pragma once

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
