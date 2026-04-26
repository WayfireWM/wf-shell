#include "wf-popover.hpp"
#include "giomm/menumodel.h"
#include "glib.h"
#include "glibmm.h"
#include "glibmm/main.h"
#include "gtk/gtk.h"
#include "gtkmm/eventcontrollermotion.h"
#include "gtkmm/gestureclick.h"
#include "gtkmm/widget.h"
#include "wf-autohide-window.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <gtk4-layer-shell.h>

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

void WayfireMenuWidget::set_no_child()
{
    remove_css_class("with-content");
    popover.popdown();
    menu.popdown();
    use_menu   = false;
    use_widget = false;
}

void WayfireMenuWidget::set_menu_model(Glib::RefPtr<Gio::MenuModel> new_menu)
{
    add_css_class("with-content");
    popover.popdown();
    use_menu   = true;
    use_widget = false;
    menu.set_menu_model(new_menu);
}

void WayfireMenuWidget::popup()
{
    cancel_timer();
    auto panel = get_panel(this);
    if (!panel)
    {
        return;
    }

    if (panel->is_active_popover(*this))
    {
        return;
    }

    if (use_menu)
    {
        menu.popup();
    } else if (use_widget)
    {
        if (fullscreen)
        {
            fullscreen->show();
        } else
        {
            popover.popup();
        }
    } else
    {
        return;
    }

    add_css_class("selected");

    panel->set_active_popover(*this);
    popup_signal.emit();
}

void WayfireMenuWidget::popup_timed(int millis)
{
    /* Timed popup is assumed to not be a user action. Do not popup if a popup is already shown */
    auto panel = get_panel(this);
    if (!panel)
    {
        return;
    }

    if (panel->is_active_popover(*this))
    {
        /* If it is us and we've got a timer, reset timer */

        if (timer_signal.connected())
        {
            set_timer(millis);
        }

        return;
    }

    if (panel->has_active_popover())
    {
        return;
    }

    popup();
    set_timer(millis);
}

void WayfireMenuWidget::popdown()
{
    cancel_timer();
    auto panel = get_panel(this);
    if (!panel)
    {
        return;
    }

    remove_css_class("selected");
    menu.popdown();
    popover.popdown();
    if (fullscreen)
    {
        fullscreen->hide();
    }

    if (!use_menu && !use_widget)
    {
        return;
    }

    popdown_signal.emit();
    panel->unset_active_popover();
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
    panel_position{section + "/position"}, class_name(class_name)
{
    /* WayfireMenuWidget gets class name directly */
    add_css_class(class_name);
    add_css_class("wf-menu");
    /* Add generic popover class to both */
    popover.add_css_class("popover");
    menu.add_css_class("popover");
    /* Add specific popover class to both */
    popover.add_css_class(class_name + "-popover");
    menu.add_css_class(class_name + "-popover");

    /* Scroller around widget popover for small screens */
    popover.set_child(scroll);
    scroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scroll.set_propagate_natural_height(true);
    scroll.set_propagate_natural_width(true);

    gtk_widget_set_parent(GTK_WIDGET(popover.gobj()), GTK_WIDGET(this->gobj()));
    gtk_widget_set_parent(GTK_WIDGET(menu.gobj()), GTK_WIDGET(this->gobj()));
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
            /* Without contents, don't switch */
            if (!use_menu && !use_widget)
            {
                return;
            }

            if (panel->has_active_popover() && !panel->is_active_popover(*this))
            {
                if (panel->get_active_popover()->timer_signal.connected())
                {
                    return;
                }

                panel->get_active_popover()->popdown();
                popup();
            }
        }
    }));
    add_controller(motion_gesture);
    signals.push_back(popover.signal_closed().connect([=] ()
    {
        popdown();
    }));

    signals.push_back(menu.signal_closed().connect([=] ()
    {
        popdown();
    }));
}

/* Most cases use class and option named the same. But not all */
WayfireMenuWidget::WayfireMenuWidget(const std::string& section, std::string name) :
    WayfireMenuWidget(section, name, name)
{}

WayfireMenuWidget::~WayfireMenuWidget()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }

    timer_signal.disconnect();
    click_signal.disconnect();

    gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
    gtk_widget_unparent(GTK_WIDGET(menu.gobj()));
}

void WayfireMenuWidget::set_keyboard_interactive(bool interactive)
{
    this->interactive = interactive;
}

bool WayfireMenuWidget::is_keyboard_interactive() const
{
    return this->interactive;
}

void WayfireMenuWidget::set_active_on_window()
{
    auto panel = get_panel(this);
    if (panel)
    {
        panel->set_active_popover(*this);
    }
}

void WayfireMenuWidget::set_popup_child(Gtk::Widget & widget)
{
    add_css_class("with-content");
    menu.popdown();
    use_menu   = false;
    use_widget = true;
    scroll.set_child(widget);
}

Gtk::Widget*WayfireMenuWidget::get_popup_child()
{
    return popover.get_child();
}

void WayfireMenuWidget::toggle()
{
    Glib::signal_idle().connect([=] ()
    {
        auto panel = get_panel(this);
        if (panel)
        {
            WayfireMenuWidget *popover = panel->get_active_popover();
            if (popover == NULL)
            {
                popup();
            } else
            {
                popover->popdown();
            }
        }

        return G_SOURCE_REMOVE;
    });
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

void WayfireMenuWidget::set_fullscreen(bool fs)
{
    auto panel = get_panel(this);
    if (!panel)
    {
        return;
    }

    if (fs && (fullscreen == nullptr))
    {
        gtk_popover_set_child(popover.gobj(), nullptr);

        /* Prepare fullscreen layer */
        fullscreen = std::make_shared<Gtk::Window>();
        fullscreen->add_css_class(class_name + "-fullscreen-popover");
        fullscreen->add_css_class(class_name + "-popover");
        fullscreen->add_css_class("fullscreen-popover");
        gtk_layer_init_for_window(fullscreen->gobj());
        gtk_layer_set_namespace(fullscreen->gobj(), "panelmenu");
        gtk_layer_set_anchor(fullscreen->gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
        gtk_layer_set_anchor(fullscreen->gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
        gtk_layer_set_anchor(fullscreen->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
        gtk_layer_set_anchor(fullscreen->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
        gtk_layer_set_layer(fullscreen->gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_keyboard_mode(fullscreen->gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        gtk_layer_set_monitor(fullscreen->gobj(), panel->get_output()->monitor->gobj());

        fullscreen->set_child(scroll);
    } else if (!fs && fullscreen)
    {
        gtk_window_set_child(fullscreen->gobj(), nullptr);
        popover.set_child(scroll);
        fullscreen->close();
        fullscreen = nullptr;
    }
}

void WayfireMenuWidget::cancel_timer()
{
    timer_signal.disconnect();
}

void WayfireMenuWidget::set_timer(int millis)
{
    timer_signal.disconnect();
    timer_signal = Glib::signal_timeout().connect([=]
    {
        popdown();
        return G_SOURCE_REMOVE;
    }, millis);
}
