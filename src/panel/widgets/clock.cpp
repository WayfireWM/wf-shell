#include <glibmm.h>
#include <iostream>
#include "clock.hpp"

void WayfireClock::init(Gtk::Box *container)
{
    button = std::make_unique<WayfireMenuButton>("panel");
    button->get_style_context()->add_class("clock");
    button->set_child(label);
    button->show();
    label.show();

    update_label();

    calendar.show();
    button->get_popover()->get_style_context()->add_class("clock-popover");
    button->get_children()[0]->get_style_context()->add_class("flat");
    button->get_popover()->set_child(calendar);
    button->get_popover()->signal_show().connect(
        sigc::mem_fun(*this, &WayfireClock::on_calendar_shown));

    container->append(*button);

    timeout = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &WayfireClock::update_label), 1);
}

void WayfireClock::on_calendar_shown()
{
    auto now = Glib::DateTime::create_now_local();

    /* GDateTime uses month in 1-12 format while GClender uses 0-11  */
    // calendar.set_month(now.get_month() - 1, now.get_year());
    calendar.select_day(now);
}

bool WayfireClock::update_label()
{
    auto time = Glib::DateTime::create_now_local();
    auto text = time.format((std::string)format);

    /* Sometimes GLib::DateTime will add leading spaces. This results in
     * unevenly balanced padding around the text, which looks quite bad.
     *
     * This could be circumvented with the modifiers the user passes to the
     * format string, * but to remove the requirement that the user does
     * something fancy, we just remove any leading spaces. */
    int i = 0;
    while (i < (int)text.length() && text[i] == ' ')
    {
        i++;
    }

    label.set_text(text.substr(i));
    return 1;
}

WayfireClock::~WayfireClock()
{
    timeout.disconnect();
}
