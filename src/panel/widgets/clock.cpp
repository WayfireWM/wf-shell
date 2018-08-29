#include <glibmm.h>
#include "clock.hpp"

void WayfireClock::init(Gtk::HBox *container, wayfire_config *config)
{
    format = config->get_section("shell")
        ->get_option("clock_format", "%e %A %H:%M");

    update_label();
    container->pack_end(label, false, false);

    timeout = Glib::signal_timeout().connect_seconds(
        sigc::bind(sigc::mem_fun(&WayfireClock::update_label), this), 1);
}

bool WayfireClock::update_label()
{
    auto time = Glib::DateTime::create_now_local();
    label.set_text(time.format(format->as_string()));

    return 1;
}

int WayfireClock::get_width()
{
    return 100;
}

WayfireClock::~WayfireClock()
{
    timeout.disconnect();
}
