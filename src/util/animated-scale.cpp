#include <glibmm.h>
#include "animated-scale.hpp"

WayfireAnimatedScale::WayfireAnimatedScale()
{
    this->signal_draw().connect_notify(
        [=] (const Cairo::RefPtr<Cairo::Context>& ctx)
    {
        if (this->current_volume.running())
        {
            value_changed.block();
            this->set_value(this->current_volume);
            value_changed.unblock();
        }
    }, true);

    value_changed = this->signal_value_changed().connect_notify([=] ()
    {
        this->current_volume.animate(this->get_value(), this->get_value());
        if (this->user_changed_callback)
        {
            this->user_changed_callback();
        }
    });
}

WayfireAnimatedScale::~WayfireAnimatedScale()
{
    value_changed.disconnect();
}

void WayfireAnimatedScale::set_target_value(double value)
{
    this->current_volume.animate(value);
    this->queue_draw();
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
