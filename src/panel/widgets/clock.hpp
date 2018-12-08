#ifndef WIDGETS_CLOCK_HPP
#define WIDGETS_CLOCK_HPP

#include "../widget.hpp"
#include <gtkmm/calendar.h>
#include <gtkmm/label.h>
#include <gtkmm/popover.h>
#include <gtkmm/menubutton.h>
#include <config.hpp>

class WayfireClock : public WayfireWidget
{
    Gtk::Label label;
    Gtk::Popover popover;
    Gtk::MenuButton menu_button;
    Gtk::Calendar calendar;

    sigc::connection timeout;
    wf_option format;
    wf_option font;
    wf_option_callback font_changed;

    void set_font();
    void on_calendar_shown();

    public:
    void init(Gtk::HBox *container, wayfire_config *config) override;
    bool update_label();

    virtual void focus_lost() override;
    ~WayfireClock();
};

#endif /* end of include guard: WIDGETS_CLOCK_HPP */
