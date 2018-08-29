#ifndef WIDGETS_CLOCK_HPP
#define WIDGETS_CLOCK_HPP

#include "../widget.hpp"
#include <gtkmm/label.h>
#include <config.hpp>

class WayfireClock : public WayfireWidget
{
    Gtk::Label label;
    sigc::connection timeout;
    wf_option format;

    public:
    void init(Gtk::HBox *container, wayfire_config *config);
    bool update_label();
    int get_width();
    ~WayfireClock();
};

#endif /* end of include guard: WIDGETS_CLOCK_HPP */
