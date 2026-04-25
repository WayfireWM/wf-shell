#pragma once

#include "giomm/menumodel.h"
#include "glibmm/refptr.h"
#include "gtkmm/scrolledwindow.h"
#include <gtkmm/widget.h>
#include <sigc++/connection.h>
#include <sigc++/signal.h>
#include <gtkmm/box.h>
#include <gtkmm/popover.h>
#include <gtkmm/popovermenu.h>
#include <memory>
#include <vector>
#include <wf-option-wrap.hpp>

using type_signal_simple = sigc::signal<void (void)>;

/**
 * A button which shows a popup on click. It adjusts the popup position
 * automatically based on panel position (valid values are "top" and "bottom")
 */
class WayfireMenuWidget : public Gtk::Box
{
  private:
    Gtk::ScrolledWindow scroll;
    Gtk::Popover popover;
    Gtk::PopoverMenu menu;
    Glib::RefPtr<Gtk::Window> fullscreen;
    bool use_menu = false, use_widget = false;
    bool interactive = true;
    WfOption<std::string> panel_position;

    /* Make the menu button active on its AutohideWindow */
    void set_active_on_window();

    friend class WayfireAutohidingWindow;

    sigc::connection click_signal, timer_signal;
    std::vector<sigc::connection> signals;
    std::string class_name;

    void cancel_timer();
    void set_timer(int millis);

    type_signal_simple popup_signal, popdown_signal;
    WfOption<bool> menus_motion{"panel/menus_change_motion"};

  public:

    void set_no_child();
    void set_menu_model(Glib::RefPtr<Gio::MenuModel> menu);
    void set_child(Gtk::Widget & widget);
    Gtk::Widget *get_child();

    void popup();
    void popup_timed(int millis);
    void popdown();

    WayfireMenuWidget(const std::string& config_section,
        const std::string name);
    WayfireMenuWidget(const std::string& config_section,
        const std::string css_name,
        const std::string option_name);
    ~WayfireMenuWidget();

    /* Called when the popup is shown */
    type_signal_simple signal_popup()
    {
        return popup_signal;
    }

    /* Called when the popup is hidden */
    type_signal_simple signal_popdown()
    {
        return popdown_signal;
    }

    /**
     * Set popup child
     */
    void set_popup_child(Gtk::Widget & child);

    Gtk::Widget *get_popup_child();

    /**
     * Set whether the popup should grab input focus when opened
     * By default, the menu button interacts with the keyboard.
     */
    void set_keyboard_interactive(bool interactive = true);

    /** @return Whether the menu button interacts with the keyboard */
    bool is_keyboard_interactive() const;

    /* Toggle the popup state of the connected popup */
    void toggle();
    /* Returns true if this is the open popup for this panel */
    bool is_popup_visible();

    /* Use a specific mouse button to open menu. Skip this if you're handling presses in-widget. If button < 0
     * then this callback is removed  */
    void open_on(int button);
    void set_fullscreen(bool fs);
};
