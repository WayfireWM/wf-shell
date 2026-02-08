#include <glibmm/main.h>
#include "timedrevealer.hpp"
#include "glib.h"

WayfireLockerTimedRevealer::WayfireLockerTimedRevealer(std::string always_option) :
    always_show(WfOption<bool>{always_option})
{
    if ((hide_timeout > 0) && !always_show)
    {
        set_reveal_child(false);
    } else
    {
        set_transition_duration(0);
        set_reveal_child(true);
    }

    auto hide_callback = [this] ()
    {
        Gtk::RevealerTransitionType type = Gtk::RevealerTransitionType::NONE;
        switch (hide_animation)
        {
          case 1:
            type = Gtk::RevealerTransitionType::CROSSFADE;
            break;

          case 2:
            type = Gtk::RevealerTransitionType::SLIDE_DOWN;
            break;

          case 3:
            type = Gtk::RevealerTransitionType::SLIDE_LEFT;
            break;

          case 4:
            type = Gtk::RevealerTransitionType::SLIDE_RIGHT;
            break;

          case 5:
            type = Gtk::RevealerTransitionType::SLIDE_UP;
            break;

          case 6:
            type = Gtk::RevealerTransitionType::SWING_DOWN;
            break;

          case 7:
            type = Gtk::RevealerTransitionType::SWING_LEFT;
            break;

          case 8:
            type = Gtk::RevealerTransitionType::SWING_RIGHT;
            break;

          case 9:
            type = Gtk::RevealerTransitionType::SWING_UP;
            break;

          default:
            type = Gtk::RevealerTransitionType::NONE;
            break;
        }

        set_transition_type(type);
    };

    hide_callback();
    hide_animation.set_callback(hide_callback);

    auto hide_duration_callback = [this] ()
    {
        set_transition_duration(hide_animation_duration);
        return G_SOURCE_REMOVE;
    };



    hide_duration_callback();
    hide_animation_duration.set_callback(hide_duration_callback);
}

WayfireLockerTimedRevealer::~WayfireLockerTimedRevealer()
{
    if (signal)
    {
        signal.disconnect();
    }
}

void WayfireLockerTimedRevealer::activity()
{
    if (signal)
    {
        signal.disconnect();
    }

    set_reveal_child(true);
    if (always_show || (hide_timeout == 0))
    {
        return;
    }

    Glib::signal_timeout().connect(
        [this] ()
    {
        set_reveal_child(false);
        return G_SOURCE_REMOVE;
    },
        hide_timeout);
}
