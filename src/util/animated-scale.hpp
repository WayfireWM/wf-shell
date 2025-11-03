#ifndef ANIMATED_SCALE_HPP
#define ANIMATED_SCALE_HPP

#include "wf-popover.hpp"
#include <gtkmm/image.h>
#include <gtkmm/scale.h>
#include <wayfire/util/duration.hpp>

/**
 * A custom scale which animates transitions when its value is changed programatically.
 */
class WayfireAnimatedScale : public Gtk::Scale
{
    wf::animation::simple_animation_t current_volume{wf::create_option(200)};
    sigc::connection value_changed;
    std::function<void()> user_changed_callback;

  public:
    WayfireAnimatedScale();

    /* Gets the current target value */
    double get_target_value() const;
    /* Set a target value to animate towards */
    void set_target_value(double value);
    /** Set the callback when the user changes the scale value */
    void set_user_changed_callback(std::function<void()> callback);
    /** Callback to animate volume control */
    gboolean update_animation(Glib::RefPtr<Gdk::FrameClock> clock);
};

#endif // ANIMATED_SCALE_HPP
