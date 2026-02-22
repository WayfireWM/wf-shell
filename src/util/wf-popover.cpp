#include "wf-popover.hpp"
#include "wf-autohide-window.hpp"

WayfireMenuButton::WayfireMenuButton(const std::string& section) :
    panel_position{section + "/position"}
{
    add_css_class("flat");

    auto cb = [=] ()
    {
        this->unset_popover();
        set_popover(m_popover);
    };
    panel_position.set_callback(cb);
    cb();

    m_popover.signal_show().connect([=]
    {
        set_active_on_window();
    });
}

void WayfireMenuButton::set_keyboard_interactive(bool interactive)
{
    this->interactive = interactive;
}

bool WayfireMenuButton::is_keyboard_interactive() const
{
    return this->interactive;
}

void WayfireMenuButton::set_has_focus(bool focus)
{
    this->has_focus = focus;
}

bool WayfireMenuButton::is_popover_focused() const
{
    return this->has_focus;
}

void WayfireMenuButton::set_active_on_window()
{
    auto window = this->get_parent();
    while (window && window->get_parent())
    {
        window = window->get_parent();
    }

    auto autohide_window = dynamic_cast<WayfireAutohidingWindow*>(window);
    if (autohide_window)
    {
        autohide_window->set_active_popover(*this);
    }
}

void WayfireMenuButton::grab_focus()
{
    set_keyboard_interactive();
    set_active_on_window(); // actually grab focus
}
