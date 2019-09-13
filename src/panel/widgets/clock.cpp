#include <glibmm.h>
#include <iostream>
#include "clock.hpp"

static const std::string default_font = "default";
void WayfireClock::init(Gtk::HBox *container, wayfire_config *config)
{
    format = config->get_section("panel")
        ->get_option("clock_format", "%e %A %H:%M");
    font = config->get_section("panel")
        ->get_option("clock_font", default_font);

    button = std::make_unique<WayfireMenuButton> (PANEL_POSITION_OPT(config));
    button->add(label);
    button->show();
    label.show();

    update_label();

    calendar.show();
    button->get_popover()->add(calendar);
    button->get_popover()->signal_show().connect_notify(
        sigc::mem_fun(this, &WayfireClock::on_calendar_shown));

    container->pack_end(*button, false, false);

    timeout = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(this, &WayfireClock::update_label), 1);

    // initially set font
    set_font();

    font_changed = [=] () {
        set_font();
    };
    font->add_updated_handler(&font_changed);
}

void WayfireClock::on_calendar_shown()
{
    auto now = Glib::DateTime::create_now_local();

    /* GDateTime uses month in 1-12 format while GClender uses 0-11  */
    calendar.select_month(now.get_month() - 1, now.get_year());
    calendar.select_day(now.get_day_of_month());
}

bool WayfireClock::update_label()
{
    auto time = Glib::DateTime::create_now_local();

    auto text = time.format(format->as_string());

    /* Sometimes GLib::DateTime will add leading spaces. This results in
     * unevenly balanced padding around the text, which looks quite bad.
     *
     * This could be circumvented with the modifiers the user passes to the
     * format string, * but to remove the requirement that the user does
     * something fancy, we just remove any leading spaces. */
    int i = 0;
    while(i < (int)text.length() && text[i] == ' ')
        i++;

    label.set_text(text.substr(i));
    return 1;
}

void WayfireClock::set_font()
{
    if (font->as_string() == default_font)
    {
        label.unset_font();
    } else
    {
        label.override_font(Pango::FontDescription(font->as_string()));
    }
}

WayfireClock::~WayfireClock()
{
    timeout.disconnect();
    font->rem_updated_handler(&font_changed);
}
