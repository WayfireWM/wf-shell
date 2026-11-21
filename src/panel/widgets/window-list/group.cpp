#include <algorithm>
#include <cmath>
#include <vector>
#include <gtkmm.h>

#include "group.hpp"
#include "gtkmm/enums.h"
#include "toplevel.hpp"
#include "window-list.hpp"
#include "gtk-utils.hpp"

namespace
{
static constexpr int DRAG_THRESHOLD = 3;
}

class WayfireToplevelGroup::impl
{
    WayfireWindowList *window_list;
    std::string group_id;
    std::vector<WayfireToplevel*> members;

    Gtk::Box button;
    Gtk::Image image;
    Gtk::Label label;

    Gtk::Popover popover;
    Gtk::Box menu_box{Gtk::Orientation::VERTICAL, 6};
    std::vector<Gtk::PopoverMenu*> row_popovers;
    std::vector<TooltipMedia*> popover_medias;
    Gtk::Box custom_tooltip_content{Gtk::Orientation::VERTICAL, 6};
    std::vector<TooltipMedia*> tooltip_medias;
    bool tooltip_active = false;

    Glib::RefPtr<Gtk::GestureDrag> drag_gesture;
    std::vector<sigc::connection> signals;
    sigc::connection button_leave_signal;

    WfOption<bool> middle_click_close{"panel/middle_click_close"};

    int grab_offset = 0;
    double grab_start_x = 0, grab_start_y = 0;
    double grab_abs_start_position = 0;
    bool drag_exceeds_threshold = false;

  public:
    impl(WayfireWindowList *window_list, const std::string& group_id)
    {
        this->window_list = window_list;
        this->group_id    = group_id;

        button.add_css_class("window-button");
        button.add_css_class("window-group-button");
        image.add_css_class("widget-icon");
        image.add_css_class("toplevel-icon");
        button.append(image);
        button.append(label);
        button.set_halign(Gtk::Align::FILL);
        button.set_hexpand(true);

        image.set_halign(Gtk::Align::CENTER);
        image.set_valign(Gtk::Align::CENTER);
        image.set_from_icon_name("window-restore-symbolic");

        label.set_ellipsize(Pango::EllipsizeMode::END);
        label.set_hexpand(true);
        label.set_halign(Gtk::Align::START);

        IconProvider::image_set_icon(image, group_id);
        label.set_text(window_list->get_app_name(group_id));

        signals.push_back(button.property_scale_factor().signal_changed()
            .connect([this] ()
        {
            IconProvider::image_set_icon(image, this->group_id);
        }));

        popover.set_parent(button);
        popover.set_child(menu_box);
        menu_box.set_margin(4);

        drag_gesture = Gtk::GestureDrag::create();
        signals.push_back(drag_gesture->signal_drag_begin().connect(
            sigc::mem_fun(*this, &WayfireToplevelGroup::impl::on_drag_begin)));
        signals.push_back(drag_gesture->signal_drag_update().connect(
            sigc::mem_fun(*this, &WayfireToplevelGroup::impl::on_drag_update)));
        signals.push_back(drag_gesture->signal_drag_end().connect(
            sigc::mem_fun(*this, &WayfireToplevelGroup::impl::on_drag_end)));
        button.add_controller(drag_gesture);

        auto motion_controller = Gtk::EventControllerMotion::create();
        button_leave_signal = motion_controller->signal_leave().connect([this] ()
        {
            unset_tooltip_medias();
        });
        button.add_controller(motion_controller);

        motion_controller = Gtk::EventControllerMotion::create();
        signals.push_back(motion_controller->signal_enter().connect([this] (double, double)
        {
            set_tooltip_medias();
        }));
        button.add_controller(motion_controller);

        signals.push_back(button.signal_query_tooltip().connect(
            [this] (int x, int y, bool keyboard_mode, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
        {
            return query_tooltip(x, y, keyboard_mode, tooltip);
        }, false));
        button.set_has_tooltip(true);

        signals.push_back(popover.signal_show().connect([this] ()
        {
            unset_tooltip_medias();
            rebuild_menu(true);
        }));
        signals.push_back(popover.signal_closed().connect([this] ()
        {
            clear_popover_medias();
        }));


        set_hide_text(window_list->is_vertical() || window_list->hide_label.value());
    }

    bool can_show_previews() const
    {
        return !window_list->list_toplevels.empty() &&
               window_list->toplevel_capture_manager &&
               (bool)window_list->live_window_previews;
    }

    void clear_popover_medias()
    {
        for (auto *media : popover_medias)
        {
            if (media)
            {
                media->unparent();
            }
        }

        popover_medias.clear();
    }

    ~impl()
    {
        unset_tooltip_medias();
        clear_popover_medias();
        clear_row_popovers();
        popover.unparent();
        button_leave_signal.disconnect();

        for (auto& signal : signals)
        {
            signal.disconnect();
        }

        if (button.get_parent() == window_list)
        {
            window_list->remove(button);
        }
    }

    void clear_row_popovers()
    {
        for (auto *ctx_popover : row_popovers)
        {
            if (ctx_popover)
            {
                gtk_widget_unparent(GTK_WIDGET(ctx_popover->gobj()));
            }
        }

        row_popovers.clear();
    }

    Gtk::Widget& get_button()
    {
        return button;
    }

    void add_toplevel(WayfireToplevel *toplevel)
    {
        if (std::find(members.begin(), members.end(), toplevel) == members.end())
        {
            members.push_back(toplevel);
        }
    }

    void remove_toplevel(WayfireToplevel *toplevel)
    {
        auto it = std::find(members.begin(), members.end(), toplevel);
        if (it != members.end())
        {
            members.erase(it);
        }

        if (tooltip_active)
        {
            unset_tooltip_medias();
            if (!members.empty())
            {
                set_tooltip_medias();
            }
        }
    }

    void set_hide_text(bool hide_text)
    {
        if (hide_text)
        {
            button.add_css_class("no-text");
            label.hide();
            image.set_hexpand(true);
        } else
        {
            button.remove_css_class("no-text");
            label.show();
            image.set_hexpand(false);
        }
    }

    void sync()
    {
        IconProvider::image_set_icon(image, group_id);
        label.set_text(window_list->get_app_name(group_id));

        bool any_activated = false;
        bool all_minimized = !members.empty();

        for (auto *t : members)
        {
            uint32_t st = t->get_state();
            if (st & WF_TOPLEVEL_STATE_ACTIVATED)
            {
                any_activated = true;
            }

            if (!(st & WF_TOPLEVEL_STATE_MINIMIZED))
            {
                all_minimized = false;
            }
        }

        if (any_activated)
        {
            button.add_css_class("activated");
        } else
        {
            button.remove_css_class("activated");
        }

        if (all_minimized)
        {
            button.add_css_class("minimized");
        } else
        {
            button.remove_css_class("minimized");
        }

        rebuild_menu(popover.is_visible());
        send_rectangle_hints();

        if (tooltip_active)
        {
            unset_tooltip_medias();
            set_tooltip_medias();
        }
    }

    void rebuild_menu(bool with_previews = false)
    {
        clear_popover_medias();
        clear_row_popovers();

        while (auto *child = menu_box.get_first_child())
        {
            menu_box.remove(*child);
        }

        bool show_previews = with_previews && can_show_previews();

        for (auto *t : members)
        {
            auto *win_btn   = Gtk::make_managed<Gtk::Button>();
            auto *entry_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
            auto *row_lbl   = Gtk::make_managed<Gtk::Label>(t->get_title());
            row_lbl->set_ellipsize(Pango::EllipsizeMode::END);
            row_lbl->set_max_width_chars(45);
            row_lbl->set_halign(Gtk::Align::START);
            row_lbl->set_hexpand(true);

            entry_box->append(*row_lbl);

            if (show_previews && t->get_ext_handle())
            {
                auto *media = Gtk::make_managed<TooltipMedia>(window_list, t->get_ext_handle());
                popover_medias.push_back(media);
                entry_box->append(*media);
            }

            win_btn->set_child(*entry_box);
            win_btn->add_css_class("flat");
            win_btn->add_css_class("window-group-item");

            if (t->get_state() & WF_TOPLEVEL_STATE_ACTIVATED)
            {
                win_btn->add_css_class("activated");
            }

            if (t->get_state() & WF_TOPLEVEL_STATE_MINIMIZED)
            {
                win_btn->add_css_class("minimized");
            }

            auto *ctx_popover = Gtk::make_managed<Gtk::PopoverMenu>();
            gtk_widget_set_parent(GTK_WIDGET(ctx_popover->gobj()), GTK_WIDGET(win_btn->gobj()));
            t->setup_popover_menu(*ctx_popover);
            row_popovers.push_back(ctx_popover);

            auto handle = t->get_handle();
            win_btn->signal_clicked().connect([this, handle] ()
            {
                auto it = window_list->toplevels.find(handle);
                if ((it != window_list->toplevels.end()) && it->second)
                {
                    it->second->activate_or_minimize();
                }

                popover.popdown();
            });

            auto click_gesture = Gtk::GestureClick::create();
            click_gesture->set_button(0);
            click_gesture->signal_released().connect(
                [this, click_gesture, handle, ctx_popover] (int, double, double)
            {
                int butt = click_gesture->get_current_button();
                auto it  = window_list->toplevels.find(handle);
                if ((it == window_list->toplevels.end()) || !it->second)
                {
                    return;
                }

                if ((butt == 2) && middle_click_close.value())
                {
                    it->second->close_window();
                } else if (butt == 3)
                {
                    ctx_popover->popup();
                }
            });
            win_btn->add_controller(click_gesture);

            auto long_press = Gtk::GestureLongPress::create();
            long_press->set_touch_only(true);
            long_press->signal_pressed().connect(
                [long_press, ctx_popover] (double, double)
            {
                long_press->set_state(Gtk::EventSequenceState::CLAIMED);
                ctx_popover->popup();
            });
            win_btn->add_controller(long_press);

            menu_box.append(*win_btn);
        }
    }

    void set_tooltip_medias()
    {
        if (tooltip_active)
        {
            return;
        }

        tooltip_active = true;
        bool show_previews = can_show_previews();

        for (auto *t : members)
        {
            auto *entry_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
            auto *title_lbl = Gtk::make_managed<Gtk::Label>(t->get_title());
            title_lbl->set_ellipsize(Pango::EllipsizeMode::END);
            title_lbl->set_max_width_chars(45);
            title_lbl->set_halign(Gtk::Align::START);
            entry_box->append(*title_lbl);

            if (show_previews && t->get_ext_handle())
            {
                auto *media = Gtk::make_managed<TooltipMedia>(window_list, t->get_ext_handle());
                tooltip_medias.push_back(media);
                entry_box->append(*media);
            }

            custom_tooltip_content.append(*entry_box);
        }
    }

    bool query_tooltip(int x, int y, bool keyboard_mode, const Glib::RefPtr<Gtk::Tooltip>& tooltip)
    {
        if (popover.is_visible() || members.empty())
        {
            return false;
        }

        if (!tooltip_active)
        {
            set_tooltip_medias();
        }

        tooltip->set_custom(custom_tooltip_content);
        return true;
    }

    void unset_tooltip_medias()
    {
        tooltip_active = false;
        tooltip_medias.clear();
        while (auto *child = custom_tooltip_content.get_first_child())
        {
            custom_tooltip_content.remove(*child);
        }
    }

    void send_rectangle_hints()
    {
        for (auto *t : members)
        {
            t->send_rectangle_hint_for_widget(button);
        }
    }

    void on_drag_begin(double _x, double _y)
    {
        grab_start_x = _x;
        grab_start_y = _y;
        window_list->set_top_widget(&button);
        const double pointer_position =
            window_list->is_vertical() ? _y : _x;
        int position =
            window_list->get_absolute_position(pointer_position, button);
        grab_abs_start_position = position;
        int widget_position =
            window_list->get_absolute_position(0, button);
        grab_offset = position - widget_position;
        drag_exceeds_threshold = false;
    }

    void on_drag_update(double _x, double _y)
    {
        double relative_position =
            window_list->is_vertical() ? (_y + grab_start_y) : (_x + grab_start_x);
        int position =
            window_list->get_absolute_position(relative_position, button);
        if (std::abs(position - grab_abs_start_position) > DRAG_THRESHOLD)
        {
            drag_exceeds_threshold = true;
        }

        auto hovered_button = window_list->get_widget_at(position);
        Gtk::Widget *before = window_list->get_widget_before(position);
        if (hovered_button)
        {
            auto allocation = hovered_button->get_allocation();
            int size   = window_list->is_vertical() ? allocation.get_height() : allocation.get_width();
            int origin = window_list->is_vertical() ? allocation.get_y() : allocation.get_x();
            int half_size = size / 2;
            int position_in_button = position - origin;
            if (position_in_button < half_size)
            {
                if (before == nullptr)
                {
                    gtk_box_reorder_child_after(
                        window_list->gobj(),
                        GTK_WIDGET(button.gobj()),
                        nullptr);
                } else
                {
                    window_list->reorder_child_after(button, *before);
                }
            } else if (position_in_button > half_size)
            {
                window_list->reorder_child_after(button, *hovered_button);
            }
        }

        int target_position = position - grab_offset;
        window_list->set_top_position(target_position);
    }

    void on_drag_end(double _x, double _y)
    {
        window_list->set_top_widget(nullptr);
        if (!drag_exceeds_threshold)
        {
            unset_tooltip_medias();
            popover.popup();
        }

        drag_gesture->set_state(Gtk::EventSequenceState::DENIED);
        send_rectangle_hints();
    }
};
WayfireToplevelGroup::WayfireToplevelGroup(WayfireWindowList *window_list, const std::string& group_id) :
    pimpl(new WayfireToplevelGroup::impl(window_list, group_id))
{}
WayfireToplevelGroup::~WayfireToplevelGroup()
{}
void WayfireToplevelGroup::add_toplevel(WayfireToplevel *toplevel)
{
    pimpl->add_toplevel(toplevel);
}

void WayfireToplevelGroup::remove_toplevel(WayfireToplevel *toplevel)
{
    pimpl->remove_toplevel(toplevel);
}

void WayfireToplevelGroup::sync()
{
    pimpl->sync();
}

void WayfireToplevelGroup::set_hide_text(bool hide_text)
{
    pimpl->set_hide_text(hide_text);
}

void WayfireToplevelGroup::send_rectangle_hints()
{
    pimpl->send_rectangle_hints();
}

Gtk::Widget& WayfireToplevelGroup::get_button()
{
    return pimpl->get_button();
}
