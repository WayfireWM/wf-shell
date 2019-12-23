#ifndef WF_PANEL_POPOVER_HPP
#define WF_PANEL_POPOVER_HPP

#include <gtkmm/menubutton.h>
#include <gtkmm/popover.h>
#include <wf-option-wrap.hpp>

/**
 * A button which shows a popover on click. It adjusts the popup position
 * automatically based on panel position (valid values are "top" and "bottom")
 */
class WayfireMenuButton : public Gtk::MenuButton
{
    bool interactive = true;
    bool has_focus = false;
    WfOption<std::string> panel_position;

    /* Make the menu button active on its AutohideWindow */
    void set_active_on_window();

    friend class WayfireAutohidingWindow;
    /* Set the has_focus property */
    void set_has_focus(bool focus);

  public:
    Gtk::Popover m_popover;

    WayfireMenuButton(const std::string& config_section);
    virtual ~WayfireMenuButton() {}

    /**
     * Set whether the popup should grab input focus when opened
     * By default, the menu button interacts with the keyboard.
     */
    void set_keyboard_interactive(bool interactive = true);

    /** @return Whether the menu button interacts with the keyboard */
    bool is_keyboard_interactive() const;

    /** @return Whether the popover currently has keyboard focus */
    bool is_popover_focused() const;

    /**
     * Grab the keyboard focus.
     * Also sets the popover to keyboard interactive.
     *
     * NOTE: this works only if the popover was already opened.
     */
    void grab_focus();
};

#endif /* end of include guard: WF_PANEL_POPOVER_HPP */
