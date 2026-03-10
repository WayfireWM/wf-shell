#pragma once
#include <gtkmm/revealer.h>
#include "wf-option-wrap.hpp"
#include <sigc++/connection.h>

class WayfireLockerTimedRevealer : public Gtk::Revealer
{
  private:
    sigc::connection signal;

  public:
    WayfireLockerTimedRevealer(std::string always_option);
    ~WayfireLockerTimedRevealer();
    WfOption<bool> always_show;
    WfOption<double> hide_timeout{"locker/hide_time"};
    WfOption<int> hide_animation{"locker/hide_anim"};
    WfOption<double> hide_animation_duration{"locker/hide_anim_dur"};

    virtual void activity(); /* Allow plugins to have their own logic if more intricate */
};
