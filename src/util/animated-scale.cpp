#include <iomanip>
#include <glibmm.h>
#include "animated-scale.hpp"
#include "gtk-utils.hpp"


WayfireAnimatedScale::WayfireAnimatedScale()
{
    value_changed = this->signal_value_changed().connect([=] ()
    {
        this->current_volume.animate(this->get_value(), this->get_value());
        if (this->user_changed_callback)
        {
            this->user_changed_callback();
        }
    });
    update_a11y();
}

WayfireAnimatedScale::~WayfireAnimatedScale()
{
    value_changed.disconnect();
}

void WayfireAnimatedScale::update_a11y()
{
    std::string percentage = get_percentage();
    if (previous_a11y != percentage)
    {
        set_a11y_prop(*this, Gtk::Accessible::Property::VALUE_TEXT, get_percentage());
        previous_a11y = percentage;
    }
}

void WayfireAnimatedScale::set_target_value(double value)
{
    this->current_volume.animate(value);
    add_tick_callback(sigc::mem_fun(*this, &WayfireAnimatedScale::update_animation));
    update_a11y();
}

gboolean WayfireAnimatedScale::update_animation(Glib::RefPtr<Gdk::FrameClock> frame_clock)
{
    value_changed.block();
    this->set_value(this->current_volume);
    value_changed.unblock();
    update_a11y();
    // Once we've finished fading, stop this callback
    return this->current_volume.running() ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

double WayfireAnimatedScale::get_target_value() const
{
    return this->current_volume.end;
}

void WayfireAnimatedScale::set_user_changed_callback(
    std::function<void()> callback)
{
    this->user_changed_callback = callback;
}

std::string WayfireAnimatedScale::get_percentage()
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(0) << (get_target_value() * 100.0) << "%";
    return oss.str();
}
