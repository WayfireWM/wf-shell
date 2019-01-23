#ifndef WF_PANEL_POPOVER_HPP
#define WF_PANEL_POPOVER_HPP

#include <config.hpp>
#include <gtkmm/menubutton.h>
#include <gtkmm/popover.h>

/* A button which shows a popover on click. It adjusts the popup position
 * automatically based on panel position (top or bottom) */
class WayfireMenuButton : public Gtk::MenuButton
{
    Gtk::Popover m_popover;

    wf_option panel_position;
    wf_option_callback panel_position_changed;

    public:
        WayfireMenuButton(wayfire_config *config);
        virtual ~WayfireMenuButton();
};

#endif /* end of include guard: WF_PANEL_POPOVER_HPP */
