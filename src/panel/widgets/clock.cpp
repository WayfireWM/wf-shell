#include <glibmm.h>
#include "clock.hpp"

static const std::string default_font = "default";
void WayfireClock::init(Gtk::HBox *container, wayfire_config *config)
{
    format = config->get_section("panel")
        ->get_option("clock_format", "%e %A %H:%M");
    font = config->get_section("panel")
        ->get_option("clock_font", default_font);

    update_label();
    container->pack_end(label, false, false);

    timeout = Glib::signal_timeout().connect_seconds(
        sigc::bind(sigc::mem_fun(&WayfireClock::update_label), this), 1);

    // initially set font
    set_font();

    font_changed = [=] () {
        set_font();
    };
    font->add_updated_handler(&font_changed);
}

bool WayfireClock::update_label()
{
    auto time = Glib::DateTime::create_now_local();
    label.set_text(time.format(format->as_string()));
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
