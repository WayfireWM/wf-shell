#pragma once

#include "gtkmm/box.h"
#include "gtkmm/builder.h"
#include "gtkmm/button.h"
#include "gtkmm/togglebutton.h"
#include <string>
#include <xkbcommon/xkbcommon.h>
class WayfireOskLayout : public Gtk::Box
{
  private:
    Glib::RefPtr<Gtk::Builder> builder;
    void hook_up();
    std::vector<sigc::connection> signals;

    void bind(Gtk::Button *button, std::string name);
    void bind_toggle(Gtk::ToggleButton *button, std::string name);
    static std::string get_keycap_symbol(std::string id);

  public:
    WayfireOskLayout(std::string file_name);
    ~WayfireOskLayout();
};
