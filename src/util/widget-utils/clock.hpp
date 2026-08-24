#pragma once

#include <gtkmm/label.h>
#include <glibmm/main.h>
#include <string>

#include "wf-option-wrap.hpp"

class ShellClock : public Gtk::Label
{
  public:
    ShellClock(const std::string& section);
    ~ShellClock();

    void update_time();

  private:
    WfOption<std::string> format_opt;
    sigc::connection timeout;
};
