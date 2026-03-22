#include "wf-popover.hpp"
#include "giomm/menumodel.h"
#include "glibmm/refptr.h"
#include "gtk/gtk.h"
#include "gtkmm/eventcontrollermotion.h"
#include "gtkmm/gestureclick.h"
#include "gtkmm/widget.h"
#include "wf-autohide-window.hpp"
#include <cstddef>
#include <memory>

/* Helper to get panel from button. NULL if not added to one */
WayfireAutohidingWindow *get_panel(Gtk::Widget *button)
{
    auto window = button->get_root();
    if (!window)
    {
        return NULL;
    }

    auto autohide_window = dynamic_cast<WayfireAutohidingWindow*>(window);
    return autohide_window;
}

WayfirePopup::WayfirePopup(std::string class_name, std::string option_name)
{
    popover.add_css_class(class_name + "-popover");

    signals.push_back(popover.signal_closed().connect([=] ()
    {
        popdown();
    }));

    signals.push_back(menu.signal_closed().connect([=] ()
    {
        popdown();
    }));
}

WayfirePopup::~WayfirePopup()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

void WayfirePopup::set_child(Gtk::Widget & widget)
{
    menu.popdown();
    use_menu = false;
    popover.set_child(scroll);
    scroll.set_child(widget);
    scroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scroll.set_propagate_natural_height(true);
    scroll.set_propagate_natural_width(true);
}

void WayfirePopup::set_menu_model(Glib::RefPtr<Gio::MenuModel> new_menu)
{
    popover.popdown();
    use_menu = true;
    menu.set_menu_model(new_menu);
}

Gtk::Widget*WayfirePopup::get_child()
{
    return popover.get_child();
}

void WayfirePopup::set_parent(Gtk::Widget & parent)
{
    gtk_widget_set_parent(GTK_WIDGET(popover.gobj()), GTK_WIDGET(parent.gobj()));
    gtk_widget_set_parent(GTK_WIDGET(menu.gobj()), GTK_WIDGET(parent.gobj()));
}

void WayfirePopup::unset_parent()
{
    gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
    gtk_widget_unparent(GTK_WIDGET(menu.gobj()));
}

void WayfirePopup::popup()
{
    if (use_menu)
    {
        menu.popup();
    } else
    {
        popover.popup();
    }
}

void WayfirePopup::popdown()
{
    if (use_menu)
    {
        menu.popdown();
    } else
    {
        popover.popdown();
    }

    auto panel = get_panel(&popover);
    if (panel)
    {
        panel->unset_active_popover();
    }
}

void WayfireMenuWidget::open_on(int button)
{
    if (click_signal)
    {
        click_signal.disconnect();
    }

    if (button < 0)
    {
        return;
    }

    auto click_gesture = Gtk::GestureClick::create();
    click_gesture->set_button(button);
    /* Action on release */
    click_signal = click_gesture->signal_released().connect(
        [=] (int, double, double)
    {
        toggle();
    });
    add_controller(click_gesture);
}

WayfireMenuWidget::WayfireMenuWidget(const std::string& section, const std::string class_name,
    const std::string option_name) :
    panel_position{section + "/position"}
{
    add_css_class(class_name);

    m_popup = std::make_shared<WayfirePopup>(class_name, option_name);
    m_popup->set_parent(*this);



    /* Moved to another menu */
    auto motion_gesture = Gtk::EventControllerMotion::create();
    signals.push_back(motion_gesture->signal_enter().connect(
        [=] (double, double)
    {
        if (!menus_motion.value())
        {
            return;
        }

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
    add_controller(motion_gesture);
}

WayfireMenuWidget::WayfireMenuWidget(const std::string& section, std::string name) :
    WayfireMenuWidget(section, name, name)
{}

WayfireMenuWidget::~WayfireMenuWidget()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }

    click_signal.disconnect();
    m_popup->unset_parent();
}

void WayfireMenuWidget::set_keyboard_interactive(bool interactive)
{
    this->interactive = interactive;
}

bool WayfireMenuWidget::is_keyboard_interactive() const
{
    return this->interactive;
}

void WayfireMenuWidget::set_has_focus(bool focus)
{
    this->has_focus = focus;
}

bool WayfireMenuWidget::is_popup_focused() const
{
    return this->has_focus;
}

void WayfireMenuWidget::set_active_on_window()
{
    auto panel = get_panel(this);
    if (panel)
    {
        panel->set_active_popover(*this);
    }
}

void WayfireMenuWidget::grab_focus()
{
    set_keyboard_interactive();
    set_active_on_window(); // actually grab focus
}

void WayfireMenuWidget::set_popup_child(Gtk::Widget & widget)
{
    m_popup->set_child(widget);
}

Gtk::Widget*WayfireMenuWidget::get_popup_child()
{
    return m_popup->get_child();
}

void WayfireMenuWidget::popup()
{
    m_popup->popup();
    popup_signal.emit();
    auto panel = get_panel(this);
    if (panel)
    {
        panel->set_active_popover(*this);
    }
}

void WayfireMenuWidget::popdown()
{
    m_popup->popdown();
    popdown_signal.emit();
    auto panel = get_panel(this);
    if (panel)
    {
        panel->unset_active_popover(*this);
    }
}

void WayfireMenuWidget::toggle()
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

bool WayfireMenuWidget::is_popup_visible()
{
    auto panel = get_panel(this);
    if (panel)
    {
        return panel->get_active_popover() == this;
    }

    return false;
}

void WayfireMenuWidget::set_menu_model(Glib::RefPtr<Gio::MenuModel> menu)
{
    m_popup->set_menu_model(menu);
}
