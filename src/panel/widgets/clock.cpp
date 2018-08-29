#include <glibmm.h>
#include "clock.hpp"

void WayfireClock::init(Gtk::Container *container)
{
    update_label();
    container->add(label);

    timeout = Glib::signal_timeout().connect_seconds(
        sigc::bind(sigc::mem_fun(&WayfireClock::update_label), this), 1);
}

bool WayfireClock::update_label()
{
    auto time = Glib::DateTime::create_now_local();
    label.set_text(time.format("%e %A %H:%M:%S"));

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
