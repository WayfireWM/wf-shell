#ifndef WIDGETS_CLOCK_HPP
#define WIDGETS_CLOCK_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"
#include <gtkmm/calendar.h>
#include <gtkmm/label.h>

class WayfireClock : public WayfireWidget
{
    Gtk::Label label;
    Gtk::Calendar calendar;
    std::unique_ptr<WayfireMenuButton> button;

    sigc::connection timeout;
    WfOption<std::string> format{"panel/clock_format"};
    WfOption<std::string> font{"panel/clock_font"};

    void set_font();
    void on_calendar_shown();

    public:
    void init(Gtk::HBox *container) override;
    bool update_label();
    ~WayfireClock();
};

#endif /* end of include guard: WIDGETS_CLOCK_HPP */
