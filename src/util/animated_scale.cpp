#include <iostream>
#include <glibmm.h>
#include "gtk-utils.hpp"
#include "animated_scale.hpp"

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
}

void WayfireAnimatedScale::set_target_value(double value)
{
    this->current_volume.animate(value);
    add_tick_callback(sigc::mem_fun(*this, &WayfireAnimatedScale::update_animation));
}

gboolean WayfireAnimatedScale::update_animation(Glib::RefPtr<Gdk::FrameClock> frame_clock)
{
    value_changed.block();
    this->set_value(this->current_volume);
    value_changed.unblock();
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

