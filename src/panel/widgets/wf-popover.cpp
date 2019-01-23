#include "wf-popover.hpp"
#include <iostream>
#include "../widget.hpp"

WayfireMenuButton::WayfireMenuButton(wayfire_config *config)
{
    get_style_context()->add_class("flat");
    m_popover.set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);

    panel_position = config->get_section("panel")->get_option(PANEL_POSITION_OPT,
        PANEL_POSITION_DEFAULT);

    panel_position_changed = [=] () {
        set_direction(panel_position->as_string() == PANEL_POSITION_TOP ?
            Gtk::ARROW_DOWN : Gtk::ARROW_UP);

        this->unset_popover();
        m_popover.set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
        set_popover(m_popover);
    };

    panel_position_changed();
    panel_position->add_updated_handler(&panel_position_changed);
}

WayfireMenuButton::~WayfireMenuButton()
{
    panel_position->rem_updated_handler(&panel_position_changed);
}
