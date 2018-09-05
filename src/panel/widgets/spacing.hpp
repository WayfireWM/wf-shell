#ifndef WIDGET_SPACING_HPP
#define WIDGET_SPACING_HPP

#include "../widget.hpp"

class WayfireSpacing : public WayfireWidget
{
    Gtk::HBox box;
    public:
        WayfireSpacing(int pixels);

        virtual void init(Gtk::HBox *container, wayfire_config *config);
        virtual ~WayfireSpacing() {}
};


#endif /* end of include guard: WIDGET_SPACING_HPP */

