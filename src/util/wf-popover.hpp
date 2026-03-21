#pragma once

#include "giomm/menumodel.h"
#include "glibmm/refptr.h"
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
 * A popup subclass for WayfireMenuButton
 */
class WayfirePopup
{
  private:
    Gtk::Popover popover;
    Gtk::PopoverMenu menu;
    bool use_menu;

    std::vector<sigc::connection> signals;

  public:
    WayfirePopup(std::string class_name, std::string option_name);
    ~WayfirePopup();

    void set_menu_model(Glib::RefPtr<Gio::MenuModel> menu);
    void set_child(Gtk::Widget & widget);
    Gtk::Widget *get_child();

    void popup();
    void popdown();

    void set_parent(Gtk::Widget & widget);
    void unset_parent();
};

/**
 * A button which shows a popup on click. It adjusts the popup position
 * automatically based on panel position (valid values are "top" and "bottom")
 */
class WayfireMenuWidget : public Gtk::Box
{
    bool interactive = true;
    bool has_focus   = false;
    WfOption<std::string> panel_position;

    /* Make the menu button active on its AutohideWindow */
    void set_active_on_window();

    friend class WayfireAutohidingWindow;
    /* Set the has_focus property */
    void set_has_focus(bool focus);

    std::shared_ptr<WayfirePopup> m_popup;

    std::vector<sigc::connection> signals;
    sigc::connection click_signal;

    type_signal_simple popup_signal, popdown_signal;

  public:
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

    /** @return Whether the popup currently has keyboard focus */
    bool is_popup_focused() const;

    /**
     * Grab the keyboard focus.
     * Also sets the popup to keyboard interactive.
     *
     * NOTE: this works only if the popup was already opened.
     */
    void grab_focus();

    /* Open the connected popup */
    void popup();
    /* Close the connected popup */
    void popdown();
    /* Toggle the popup state of the connected popup */
    void toggle();
    /* Returns true if this is the open popup for this panel */
    bool is_popup_visible();

    /* Use a specific mouse button to open menu. Skip this if you're handling presses in-widget. If button < 0
     * then this callback is removed  */
    void open_on(int button);

    /* Set the MenuModel of the popup. Masks the child set by set_popup_child */
    void set_menu_model(Glib::RefPtr<Gio::MenuModel>);
};
