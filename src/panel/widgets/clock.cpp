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

    popover = Gtk::Popover(menu_button);
    menu_button.add(label);
    menu_button.set_direction(Gtk::ARROW_DOWN);
    menu_button.set_popover(popover);
    menu_button.get_style_context()->add_class("flat");

    popover.set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);

    update_label();

    //popover.set_position(Gtk::POS_BOTTOM);
    calendar.show();
    popover.add(calendar);
    popover.signal_show().connect_notify(sigc::mem_fun(this, &WayfireClock::on_calendar_shown));

    container->pack_end(menu_button, false, false);

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

    calendar.select_month(now.get_month(), now.get_year());
    calendar.select_day(now.get_day_of_month());
}

void WayfireClock::focus_lost()
{
    menu_button.set_active(false);
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
