#ifndef WIDGET_SEPARATOR_HPP
#define WIDGET_SEPARATOR_HPP

#include "../widget.hpp"
#include <gtkmm/separator.h>

class WayfireSeparator : public WayfireWidget
{
    Gtk::Separator separator;

  public:
    WayfireSeparator(int pixels);

    virtual void init(Gtk::Box *container);

    void update_layout();
    void handle_config_reload();

    virtual ~WayfireSeparator()
    {}
};


#endif /* end of include guard: WIDGET_SEPARATOR_HPP */
