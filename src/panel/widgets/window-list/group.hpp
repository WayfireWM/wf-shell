#pragma once

#include <memory>
#include <string>
#include <gtkmm/widget.h>

class WayfireWindowList;
class WayfireToplevel;

/* Represents a group of windows sharing the same app_id */
class WayfireToplevelGroup
{
  public:
    WayfireToplevelGroup(WayfireWindowList *window_list, const std::string& group_id);
    ~WayfireToplevelGroup();

    void add_toplevel(WayfireToplevel *toplevel);
    void remove_toplevel(WayfireToplevel *toplevel);
    void sync();
    void set_hide_text(bool hide_text);
    void send_rectangle_hints();
    Gtk::Widget& get_button();

    class impl;

  private:
    std::unique_ptr<impl> pimpl;
};
