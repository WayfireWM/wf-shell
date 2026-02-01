#include <gtkmm.h>
#include <wp/proxy-interfaces.h>
#include <wp/proxy.h>

#include "wp-mixer.hpp"
#include "wf-wp-control.hpp"
#include "../volume-level.hpp"

bool WayfireWpMixer::on_popover_timeout(int timer)
{
    popover_timeout.disconnect();
    popover->popdown();
    return false;
}

void WayfireWpMixer::check_set_popover_timeout()
{
    popover_timeout.disconnect();

    popover_timeout = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
        &WayfireWpMixer::on_popover_timeout), 0), timeout * 1000);
}

void WayfireWpMixer::cancel_popover_timeout()
{
    popover_timeout.disconnect();
}

void WayfireWpMixer::reload_config()
{
    // adjust margins and spacing
    static WfOption<int> spacing{"panel/wp_spacing"};
    auto set_spacing = [&] (Gtk::Box& box)
    {
        box.set_spacing(spacing);
        box.set_margin_top(spacing);
        box.set_margin_bottom(spacing);
    };

    set_spacing(sinks_box);
    set_spacing(sources_box);
    set_spacing(streams_box);

    // big matching operation
    static WfOption<std::string> str_quick_target_choice{"panel/wp_quick_target_choice"};
    static WfOption<std::string> str_wp_left_click_action{"panel/wp_left_click_action"};
    static WfOption<std::string> str_wp_right_click_action{"panel/wp_right_click_action"};
    static WfOption<std::string> str_wp_middle_click_action{"panel/wp_middle_click_action"};

    if (str_quick_target_choice.value() == "last_change")
    {
        quick_target_choice = QuickTargetChoice::LAST_CHANGE;
    } else if (str_quick_target_choice.value() == "default_sink")
    {
        quick_target_choice = QuickTargetChoice::DEFAULT_SINK;
        WpCommon::get().re_evaluate_def_nodes();
    } else if (str_quick_target_choice.value() == "default_source")
    {
        quick_target_choice = QuickTargetChoice::DEFAULT_SOURCE;
        WpCommon::get().re_evaluate_def_nodes();
    } else // default if no match
    {
        quick_target_choice = QuickTargetChoice::LAST_CHANGE;
    }

    // "actions" that can be bound to different clicks

    auto show_mixer_action = [&] (int c, double x, double y)
    {
        // unschedule hiding
        cancel_popover_timeout();

        if ((popover->get_child() == (Gtk::Widget*)&master_box) && popover->is_visible())
        {
            popover->popdown();
            return;
        }

        if (!popover->is_visible())
        {
            button->set_active(true);
        }

        if (popover->get_child() != (Gtk::Widget*)&master_box)
        {
            popover->set_child(master_box);
            popover_timeout.disconnect();
        }
    };

    auto show_quick_target_action = [&] (int c, double x, double y)
    {
        // unschedule hiding
        cancel_popover_timeout();
        if (!quick_target)
        {
            return; // no quick_target means we have nothing to show
        }

        if ((popover->get_child() == quick_target.get()) && popover->is_visible())
        {
            popover->popdown();
            return;
        }

        if (!popover->is_visible())
        {
            button->set_active(true);
        }

        if (popover->get_child() != quick_target.get())
        {
            popover->set_child(*quick_target);
            popover_timeout.disconnect();
        }
    };

    auto mute_action = [&] (int c, double x, double y)
    {
        if (!quick_target)
        {
            return; // no quick_target means we have nothing to change by clicking
        }

        quick_target->button.set_active(!quick_target->button.get_active());
    };

    // the left click case is a bit special, since it’s supposed to show the popover.
    // (this is also why the mute action is not available for the left click)
    if (str_wp_left_click_action.value() == "show_mixer")
    {
        left_conn = left_click_gesture->signal_pressed().connect(
            [&] (int c, double x, double y)
        {
            // unschedule hiding
            cancel_popover_timeout();
            if (popover->get_child() != (Gtk::Widget*)&master_box)
            {
                popover->set_child(master_box);
                // popdown so that when the click is processed, the popover is down, and thus pops up
                // not the prettiest result, as it visibly closes instead of just replacing, but i’m not sure
                // how to make it better
                button->set_active(false);
            }
        });
    }

    if (str_wp_left_click_action.value() == "show_quick_target")
    {
        left_conn = left_click_gesture->signal_pressed().connect(
            [&] (int c, double x, double y)
        {
            // unschedule hiding
            cancel_popover_timeout();
            if (!quick_target)
            {
                return;
            }

            if (popover->get_child() != quick_target.get())
            {
                popover->set_child(*quick_target);
                // same as above
                button->set_active(false);
            }
        });
    }

    // more simple matching

    if (str_wp_middle_click_action.value() == "show_mixer")
    {
        middle_conn = middle_click_gesture->signal_pressed().connect(show_mixer_action);
    }

    if (str_wp_middle_click_action.value() == "show_quick_target")
    {
        middle_conn = middle_click_gesture->signal_pressed().connect(show_quick_target_action);
    }

    if (str_wp_middle_click_action.value() == "mute_quick_target")
    {
        middle_conn = middle_click_gesture->signal_pressed().connect(mute_action);
    }

    if (str_wp_right_click_action.value() == "show_mixer")
    {
        right_conn = right_click_gesture->signal_pressed().connect(show_mixer_action);
    }

    if (str_wp_right_click_action.value() == "show_quick_target")
    {
        right_conn = right_click_gesture->signal_pressed().connect(show_quick_target_action);
    }

    if (str_wp_right_click_action.value() == "mute_quick_target")
    {
        right_conn = right_click_gesture->signal_pressed().connect(mute_action);
    }
}

void WayfireWpMixer::handle_config_reload()
{
    left_conn.disconnect();
    middle_conn.disconnect();
    right_conn.disconnect();
    reload_config();
    for (auto & entry : objects_to_controls)
    {
        auto & control = entry.second;
        control->handle_config_reload();
    }
}

void WayfireWpMixer::init(Gtk::Box *container)
{
    // sets up the "widget part"

    button = std::make_unique<WayfireMenuButton>("panel");
    button->add_css_class("widget-icon");
    button->add_css_class("wireplumber");
    button->add_css_class("flat");
    button->get_children()[0]->add_css_class("flat");
    button->set_child(main_image);
    button->show();
    sinks_box.add_css_class("outputs");
    sources_box.add_css_class("inputs");
    streams_box.add_css_class("streams");
    out_in_wall.add_css_class("out-in");
    in_streams_wall.add_css_class("in_streams");

    popover = button->get_popover();
    popover->set_child(master_box);
    popover->set_autohide(false);
    popover->add_css_class("wireplumber-popover");

    // scroll to change volume of the object targetted by the quick_target widget
    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_conn = scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        if (!quick_target)
        {
            return false; // no quick_target means we have nothing to change by scrolling
        }

        dy = invert_scroll ? dy : dy * -1; // for the same scrolling as volume widget, which we will agree it
                                           // is more intuitive for more people
        double change = 0;
        const double SCROLL_MULT = 0.2; // corrects the scrolling to have the default scroll sensitivity as 1
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = (dy * scroll_sensitivity * SCROLL_MULT) / 10;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy * scroll_sensitivity * SCROLL_MULT) / 100;
        }

        guint32 id = wp_proxy_get_bound_id(WP_PROXY(quick_target->object));
        WpCommon::get().set_volume(id, quick_target->get_scale_target_value() + change);
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
    button->add_controller(scroll_gesture);

    // set up gestures
    left_click_gesture   = Gtk::GestureClick::create();
    right_click_gesture  = Gtk::GestureClick::create();
    middle_click_gesture = Gtk::GestureClick::create();
    left_click_gesture->set_button(1);
    middle_click_gesture->set_button(2);
    right_click_gesture->set_button(3);
    button->add_controller(left_click_gesture);
    button->add_controller(right_click_gesture);
    button->add_controller(middle_click_gesture);

    // config options
    reload_config();

    // boxes hierarchy and labeling
    master_box.set_orientation(Gtk::Orientation::HORIZONTAL);

    // column orientations
    out_in_wall.set_orientation(Gtk::Orientation::VERTICAL);
    in_streams_wall.set_orientation(Gtk::Orientation::VERTICAL);

    // assemble master box (todo: only show boxes that have contents)
    master_box.append(sinks_box);
    master_box.append(out_in_wall);
    master_box.append(sources_box);
    master_box.append(in_streams_wall);
    master_box.append(streams_box);

    // sinks
    output_label.set_text("Output devices");
    sinks_box.append(output_label);
    out_sep.set_orientation(Gtk::Orientation::HORIZONTAL);
    sinks_box.append(out_sep);
    sinks_box.set_orientation(Gtk::Orientation::VERTICAL);

    // sources
    input_label.set_text("Input devices");
    sources_box.append(input_label);
    in_sep.set_orientation(Gtk::Orientation::HORIZONTAL);
    sources_box.append(in_sep);
    sources_box.set_orientation(Gtk::Orientation::VERTICAL);

    // streams
    streams_label.set_text("Audio streams");
    streams_box.append(streams_label);
    streams_sep.set_orientation(Gtk::Orientation::HORIZONTAL);
    streams_box.append(streams_sep);
    streams_box.set_orientation(Gtk::Orientation::VERTICAL);

    // add to the actual container
    container->append(*button);
    button->set_child(main_image);

    // if there is no audio device nor application, the quick target will not be set
    // and the widget will apear empty. Calling this here to always have the OOR icon.
    update_icon();

    // get() returns WpCommon and creates it if it wasn’t yet.
    // add_widget also catches up this widget to every audio channel registered before.
    WpCommon::get().add_widget(this);
}

void WayfireWpMixer::update_icon()
{
    // depends on quick_target widget
    if (!quick_target)
    {
        main_image.set_from_icon_name(volume_icon_for(-1)); // OOR
        return;
    }

    if (quick_target->button.get_active())
    {
        main_image.set_from_icon_name(volume_icon_for(0)); // mute
        return;
    }

    main_image.set_from_icon_name(volume_icon_for(quick_target->get_scale_target_value()));
}

void WayfireWpMixer::set_quick_target_from(WfWpControl *from)
{
    quick_target = from->copy();
    quick_target->init();
    button->set_tooltip_text(quick_target->label.get_text());
}

WayfireWpMixer::~WayfireWpMixer()
{
    WpCommon::get().rem_widget(this);
    gtk_widget_unparent(GTK_WIDGET(popover->gobj()));
    popover_timeout.disconnect();
    volume_changed_signal.disconnect();
    left_conn.disconnect();
    middle_conn.disconnect();
    right_conn.disconnect();
    scroll_conn.disconnect();
}
