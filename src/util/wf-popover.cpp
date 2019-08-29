#include "wf-popover.hpp"
#include "wf-autohide-window.hpp"
#include <iostream>

WayfireMenuButton::WayfireMenuButton(wf_option panel_position)
{
    get_style_context()->add_class("flat");
    m_popover.set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);

    this->panel_position = panel_position;
    panel_position_changed = [=] () {
        set_direction(panel_position->as_string() == "top" ?
            Gtk::ARROW_DOWN : Gtk::ARROW_UP);

        this->unset_popover();
        m_popover.set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
        set_popover(m_popover);
    };

    panel_position_changed();
    panel_position->add_updated_handler(&panel_position_changed);

    m_popover.signal_show().connect_notify([=] {
        auto window = this->get_parent();
        while (window && window->get_parent())
            window = window->get_parent();

        auto autohide_window = dynamic_cast<WayfireAutohidingWindow*> (window);
        if (autohide_window)
            autohide_window->set_active_popover(*this);
    });
}

WayfireMenuButton::~WayfireMenuButton()
{
    panel_position->rem_updated_handler(&panel_position_changed);
}
