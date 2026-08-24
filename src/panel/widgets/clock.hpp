#pragma once

#include <gtkmm/calendar.h>

#include "../widget.hpp"
#include "wf-popover.hpp"
#include "widget-utils/clock.hpp"

class WayfireClock : public WayfireWidget
{
    ShellClock label{"panel/clock_format"};
    Gtk::Calendar calendar;
    std::unique_ptr<WayfireMenuWidget> button;

    sigc::connection btn_sig;
    void on_calendar_shown();

  public:
    void init(Gtk::Box *container) override;
    ~WayfireClock();
};
