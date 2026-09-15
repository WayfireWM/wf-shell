#include "clock.hpp"

ShellClock::ShellClock(const std::string& section) :
    format_opt(section)
{
    set_justify(Gtk::Justification::CENTER);
    update_time();

    timeout = Glib::signal_timeout().connect_seconds(
        [this] ()
    {
        update_time();
        return G_SOURCE_CONTINUE;
    }, 1);
}

ShellClock::~ShellClock()
{
    timeout.disconnect();
}

void ShellClock::update_time()
{
    auto time = Glib::DateTime::create_now_local();
    auto text = time.format(format_opt.value());

    /* Sometimes GLib::DateTime will add leading spaces. This results in
     * unevenly balanced padding around the text, which looks quite bad.
     *
     * This could be circumvented with the modifiers the user passes to the
     * format string, but to remove the requirement that the user does
     * something fancy, we just remove any leading spaces. */
    size_t i = 0;
    while (i < text.length() && text[i] == ' ')
    {
        i++;
    }

    set_text(text.substr(i));
}
