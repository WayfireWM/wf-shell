#include <glibmm.h>
#include "clock.hpp"


void WayfireClock::init(Gtk::Box *container)
{
    button = std::make_unique<WayfireMenuWidget>("panel", "clock");
    button->add_css_class("clock");
    button->set_child(label);
    button->open_on(1);
    label.show();

    calendar.show();
    button->set_popup_child(calendar);
    btn_sig = button->signal_popup().connect(
        sigc::mem_fun(*this, &WayfireClock::on_calendar_shown));

    container->append(*button);
}

void WayfireClock::on_calendar_shown()
{
    auto now = Glib::DateTime::create_now_local();

    /* GDateTime uses month in 1-12 format while GClender uses 0-11  */
    // calendar.set_month(now.get_month() - 1, now.get_year());
    calendar.select_day(now);
}

WayfireClock::~WayfireClock()
{
    btn_sig.disconnect();
}
