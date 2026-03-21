#include "wf-popover.hpp"
#include "gtkmm/eventcontrollermotion.h"
#include "gtkmm/gesture.h"
#include "gtkmm/gestureclick.h"
#include "gtkmm/widget.h"
#include "wf-autohide-window.hpp"
#include <cstddef>
#include <memory>

/* Helper to get panel from button. NULL if not added to one */
WayfireAutohidingWindow *get_panel(WayfireMenuButton *button)
{
    auto window = button->get_root();
    if (!window)
    {
        return NULL;
    }

    auto autohide_window = dynamic_cast<WayfireAutohidingWindow*>(window);
    return autohide_window;
}

WayfirePopover::WayfirePopover(std::string class_name, std::string option_name)
{
    popover.add_css_class(class_name + "-popover");
}

void WayfirePopover::set_child(Gtk::Widget & widget)
{
    popover.set_child(widget);
}

Gtk::Widget*WayfirePopover::get_child()
{
    return popover.get_child();
}

void WayfirePopover::set_parent(Gtk::Widget & parent)
{
    gtk_widget_set_parent(GTK_WIDGET(popover.gobj()), GTK_WIDGET(parent.gobj()));
}

void WayfirePopover::unset_parent()
{
    gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
}

void WayfirePopover::popup()
{
    popover.popup();
}

void WayfirePopover::popdown()
{
    popover.popdown();
}

WayfireMenuButton::WayfireMenuButton(const std::string& section, const std::string class_name,
    const std::string option_name) :
    panel_position{section + "/position"}
{
    add_css_class(class_name);

    m_popover = std::make_shared<WayfirePopover>(class_name, option_name);
    m_popover->set_parent(*this);

    auto click_gesture = Gtk::GestureClick::create();
    click_gesture->set_button(1);
    /* Catch a press-start */
    signals.push_back(click_gesture->signal_pressed().connect(
        [=] (int btn, double x, double y)
    {
        click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
    }));
    /* Action on release */
    signals.push_back(click_gesture->signal_released().connect(
        [=] (int, double, double)
    {
        toggle();
    }));

    /* Moved to another menu */
    auto motion_gesture = Gtk::EventControllerMotion::create();
    signals.push_back(motion_gesture->signal_enter().connect(
        [=] (double, double)
    {
        auto panel = get_panel(this);
        if (panel)
        {
            auto current = panel->get_active_popover();
            if ((current != nullptr) && (current != this))
            {
                panel->get_active_popover()->popdown();
                popup();
            }
        }
    }));


    add_controller(click_gesture);
    add_controller(motion_gesture);
}

WayfireMenuButton::WayfireMenuButton(const std::string& section, std::string name) :
    WayfireMenuButton(section, name, name)
{}

WayfireMenuButton::~WayfireMenuButton()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }

    m_popover->unset_parent();
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
    auto panel = get_panel(this);
    if (panel)
    {
        panel->set_active_popover(*this);
    }
}

void WayfireMenuButton::grab_focus()
{
    set_keyboard_interactive();
    set_active_on_window(); // actually grab focus
}

void WayfireMenuButton::set_popover_child(Gtk::Widget & widget)
{
    m_popover->set_child(widget);
}

Gtk::Widget*WayfireMenuButton::get_popover_child()
{
    return m_popover->get_child();
}

void WayfireMenuButton::popup()
{
    m_popover->popup();
    popup_signal.emit();
    auto panel = get_panel(this);
    if (panel)
    {
        panel->set_active_popover(*this);
    }
}

void WayfireMenuButton::popdown()
{
    m_popover->popdown();
    popdown_signal.emit();
}

void WayfireMenuButton::toggle()
{
    auto panel = get_panel(this);
    if (panel)
    {
        if (panel->get_active_popover() == NULL)
        {
            popup();
        } else
        {
            popdown();
        }
    }
}

bool WayfireMenuButton::is_popup_visible()
{
    auto panel = get_panel(this);
    if (panel)
    {
        return panel->get_active_popover() == this;
    }

    return false;
}
