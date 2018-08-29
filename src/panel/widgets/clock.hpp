#ifndef WIDGETS_CLOCK_HPP
#define WIDGETS_CLOCK_HPP

#include "../widget.hpp"
#include <gtkmm/label.h>

class WayfireClock : public WayfireWidget
{
    Gtk::Label label;
    sigc::connection timeout;

    public:
    void init(Gtk::Container *container);
    bool update_label();
    int get_width();
    ~WayfireClock();
};

#endif /* end of include guard: WIDGETS_CLOCK_HPP */
