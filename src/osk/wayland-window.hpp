#pragma once

#include "gtkmm/menubutton.h"
#include "wf-option-wrap.hpp"
#include <gtkmm.h>
#include <vector>
#include <wayfire-shell-unstable-v2-client-protocol.h>
#include <virtual-keyboard-unstable-v1-client-protocol.h>
#include <input-method-unstable-v2-client-protocol.h>

#define OSK_SPACING 8

class WaylandWindow : public Gtk::Window
{
    std::vector<sigc::connection> signals;
    zwf_surface_v2 *wf_surface = nullptr;

    Gtk::Widget *current_widget = nullptr;
    Gtk::Button close_button;
    Gtk::Button top_button;
    Gtk::Button bottom_button;
    Gtk::MenuButton layout_select;

    Gtk::Box suggestions;
    Gtk::Box headerbar_box;
    Gtk::Box layout_box;

    std::atomic<uint64_t> last_suggestion{0};

    WfOption<bool> exclusion{"osk/exclusion"};

    int32_t check_anchor(std::string anchor);
    void init(int width, int height, std::string anchor);
    void init_headerbar(int headerbar_size);

  public:
    WaylandWindow(int width, int height, std::string anchor, int headerbar_size);
    ~WaylandWindow();
    void set_widget(Gtk::Widget& w);
    void clear_suggestions();
    void set_suggestions(std::vector<std::string> string_suggestions, uint64_t seq_id);
};
