#include <gtkmm/menu.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/gesturedrag.h>
#include <giomm/desktopappinfo.h>
#include <iostream>

#include <gdkmm/seat.h>
#include <gdk/gdkwayland.h>
#include <cmath>


#include "toplevel.hpp"
#include "gtk-utils.hpp"
#include "panel.hpp"
#include <cassert>

namespace
{
    extern zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl;
}

namespace IconProvider
{
    void set_image_from_icon(Gtk::Image& image,
        std::string app_id_list, int size, int scale);
}

class WayfireToplevel::impl
{
    zwlr_foreign_toplevel_handle_v1 *handle, *parent;
    std::vector<zwlr_foreign_toplevel_handle_v1 *> children;
    uint32_t state;

    Gtk::Button button;
    Gtk::HBox button_contents;
    Gtk::Image image;
    Gtk::Label label;
    Gtk::Menu menu;
    Gtk::MenuItem minimize, maximize, close;
    Glib::RefPtr<Gtk::GestureDrag> drag_gesture;

    Glib::ustring app_id, title;
    public:
    WayfireWindowList *window_list;

    impl(WayfireWindowList *window_list, zwlr_foreign_toplevel_handle_v1 *handle)
    {
        this->handle = handle;
        this->parent = nullptr;
        zwlr_foreign_toplevel_handle_v1_add_listener(handle,
            &toplevel_handle_v1_impl, this);

        button_contents.add(image);
        button_contents.add(label);
        button_contents.set_halign(Gtk::ALIGN_START);
        button.add(button_contents);
        button.set_tooltip_text("none");

        button.signal_clicked().connect_notify(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_clicked));
        button.signal_size_allocate().connect_notify(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_allocation_changed));
        button.property_scale_factor().signal_changed()
            .connect(sigc::mem_fun(this, &WayfireToplevel::impl::on_scale_update));
        button.signal_button_press_event().connect(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_button_press_event));

        minimize.set_label("Minimize");
        maximize.set_label("Maximize");
        close.set_label("Close");
        minimize.signal_activate().connect(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_menu_minimize));
        maximize.signal_activate().connect(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_menu_maximize));
        close.signal_activate().connect(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_menu_close));
        menu.attach(minimize, 0, 1, 0, 1);
        menu.attach(maximize, 0, 1, 1, 2);
        menu.attach(close, 0, 1, 2, 3);

        drag_gesture = Gtk::GestureDrag::create(button);
        drag_gesture->signal_drag_begin().connect_notify(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_drag_begin));
        drag_gesture->signal_drag_update().connect_notify(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_drag_update));
        drag_gesture->signal_drag_end().connect_notify(
            sigc::mem_fun(this, &WayfireToplevel::impl::on_drag_end));

        this->window_list = window_list;
    }

    int grab_off_x;
    double grab_start_x, grab_start_y;
    double grab_abs_start_x;
    bool drag_exceeds_threshold;

    void on_drag_begin(double _x, double _y)
    {
        auto& container = window_list->box;
        /* Set grab start, before transforming it to absolute position */
        grab_start_x = _x;
        grab_start_y = _y;

        set_flat_class(false);
        window_list->box.set_top_widget(&button);

        /* Find the distance between pointer X and button origin */
        int x = container.get_absolute_position(_x, button);
        grab_abs_start_x = x;

        /* Find button corner in window-relative coords */
        int loc_x = container.get_absolute_position(0, button);
        grab_off_x = x - loc_x;

        drag_exceeds_threshold = false;
    }

    static constexpr int DRAG_THRESHOLD = 3;
    void on_drag_update(double _x, double)
    {
        auto& container = window_list->box;
        /* Window was not just clicked, but also dragged. Ignore the next click,
         * which is the one that happens when the drag gesture ends. */
        set_ignore_next_click();

        int x = _x + grab_start_x;
        x = container.get_absolute_position(x, button);
        if (std::abs(x - grab_abs_start_x) > DRAG_THRESHOLD)
            drag_exceeds_threshold = true;

        auto hovered_button = container.get_widget_at(x);

        if (hovered_button != &button && hovered_button)
        {
            auto children = container.get_unsorted_widgets();
            auto it = std::find(children.begin(), children.end(), hovered_button);
            container.reorder_child(button, it - children.begin());
        }

        /* Make sure the grabbed button always stays at the same relative position
         * to the DnD position */
        int target_x = x - grab_off_x;
        window_list->box.set_top_x(target_x);
    }

    void on_drag_end(double _x, double _y)
    {
        int x = _x + grab_start_x;
        int y = _y + grab_start_y;
        int width = button.get_allocated_width();
        int height = button.get_allocated_height();

        window_list->box.set_top_widget(nullptr);
        set_flat_class(!(state & WF_TOPLEVEL_STATE_ACTIVATED));

        /* When a button is dropped after dnd, we ignore the unclick
         * event so action doesn't happen in addition to dropping.
         * If the drag ends and the unclick event happens outside
         * the button, unset ignore_next_click or else the next
         * click on the button won't cause action. */
        if (x < 0 || x > width || y < 0 || y > height)
            unset_ignore_next_click();

        /* When dragging with touch or pen, we allow some small movement while
         * still counting the action as button press as opposed to only dragging. */
        if (!drag_exceeds_threshold)
            unset_ignore_next_click();
    }

    bool on_button_press_event(GdkEventButton* event)
    {
        if ((event->type == GDK_BUTTON_PRESS) && (event->button == 3))
        {
            if(!menu.get_attach_widget())
                menu.attach_to_widget(button);

            menu.popup(event->button, event->time);
            menu.show_all();
            return true; //It has been handled.
        }
        else
            return false;
    }

    void on_menu_minimize()
    {
        menu.popdown();
        if (state & WF_TOPLEVEL_STATE_MINIMIZED)
            zwlr_foreign_toplevel_handle_v1_unset_minimized(handle);
        else
            zwlr_foreign_toplevel_handle_v1_set_minimized(handle);
    }

    void on_menu_maximize()
    {
        menu.popdown();
        if (state & WF_TOPLEVEL_STATE_MAXIMIZED)
            zwlr_foreign_toplevel_handle_v1_unset_maximized(handle);
        else
            zwlr_foreign_toplevel_handle_v1_set_maximized(handle);
    }

    void on_menu_close()
    {
        menu.popdown();
        zwlr_foreign_toplevel_handle_v1_close(handle);
    }

    bool ignore_next_click = false;
    void set_ignore_next_click()
    {
        ignore_next_click = true;

        /* Make sure that the view doesn't show clicked on animations while
         * dragging (this happens only on some themes) */
        button.set_state_flags(Gtk::STATE_FLAG_SELECTED |
            Gtk::STATE_FLAG_DROP_ACTIVE | Gtk::STATE_FLAG_PRELIGHT);
    }

    void unset_ignore_next_click()
    {
        ignore_next_click = false;
        button.unset_state_flags(Gtk::STATE_FLAG_SELECTED |
            Gtk::STATE_FLAG_DROP_ACTIVE | Gtk::STATE_FLAG_PRELIGHT);
    }

    void on_clicked()
    {
        /* If the button was dragged, we don't want to register the click.
         * Subsequent clicks should be handled though. */
        if (ignore_next_click)
        {
            unset_ignore_next_click();
            return;
        }

        bool child_activated = false;
        for (auto c : get_children())
        {
            if (window_list->toplevels[c]->get_state() & WF_TOPLEVEL_STATE_ACTIVATED)
            {
                    child_activated = true;
                    break;
            }
        }

        if (!(state & WF_TOPLEVEL_STATE_ACTIVATED) && !child_activated)
        {
            auto gseat = Gdk::Display::get_default()->get_default_seat();
            auto seat = gdk_wayland_seat_get_wl_seat(gseat->gobj());
            zwlr_foreign_toplevel_handle_v1_activate(handle, seat);
        }
        else
        {
            send_rectangle_hint();
            if (state & WF_TOPLEVEL_STATE_MINIMIZED)
                zwlr_foreign_toplevel_handle_v1_unset_minimized(handle);
            else
                zwlr_foreign_toplevel_handle_v1_set_minimized(handle);
        }
    }

    void on_allocation_changed(Gtk::Allocation& alloc)
    {
        send_rectangle_hint();
        window_list->scrolled_window.queue_allocate();
    }

    void on_scale_update()
    {
        set_app_id(app_id);
    }

    void set_app_id(std::string app_id)
    {
        this->app_id = app_id;
        IconProvider::set_image_from_icon(image, app_id,
            24, button.get_scale_factor());
    }

    void send_rectangle_hint()
    {
        Gtk::Widget *widget = &this->button;

        int x = 0, y = 0;
        int width = button.get_allocated_width();
        int height = button.get_allocated_height();

        while (widget)
        {
            x += widget->get_allocation().get_x();
            y += widget->get_allocation().get_y();
            widget = widget->get_parent();
        }

        auto panel =
            WayfirePanelApp::get().panel_for_wl_output(window_list->output->wo);
        if (!panel)
            return;

        zwlr_foreign_toplevel_handle_v1_set_rectangle(handle,
            panel->get_wl_surface(), x, y, width, height);
    }

    int32_t max_width = 0;
    void set_title(std::string title)
    {
        this->title = title;
        button.set_tooltip_text(title);

        set_max_width(max_width);
    }

    Glib::ustring shorten_title(int show_chars)
    {
        if (show_chars == 0)
            return "";

        int title_len = title.length();
        Glib::ustring short_title = title.substr(0, show_chars);
        if (title_len - show_chars >= 2) {
            short_title += "..";
        } else if (title_len != show_chars) {
            short_title += ".";
        }

        return short_title;
    }

    int get_button_preferred_width()
    {
        int min_width, preferred_width;
        button.get_preferred_width(min_width, preferred_width);

        return preferred_width;
    }

    void set_max_width(int width)
    {
        this->max_width = width;
        if (max_width == 0)
        {
            this->button.set_size_request(-1, -1);
            this->label.set_label(title);
            return;
        }

        this->button.set_size_request(width, -1);

        int show_chars = 0;
        for (show_chars = title.length(); show_chars > 0; show_chars--)
        {
            this->label.set_text(shorten_title(show_chars));
            if (get_button_preferred_width() <= max_width)
                break;
        }

        label.set_text(shorten_title(show_chars));
    }

    uint32_t get_state()
    {
        return this->state;
    }

    zwlr_foreign_toplevel_handle_v1 * get_parent()
    {
        return this->parent;
    }

    void set_parent(zwlr_foreign_toplevel_handle_v1 *parent)
    {
        this->parent = parent;
    }

    std::vector<zwlr_foreign_toplevel_handle_v1 *>& get_children()
    {
        return this->children;
    }

    void remove_button()
    {
        auto& container = window_list->box;
        container.remove(button);
    }

    void update_menu_item_text()
    {
        if (state & WF_TOPLEVEL_STATE_MINIMIZED)
            minimize.set_label("Unminimize");
        else
            minimize.set_label("Minimize");

        if (state & WF_TOPLEVEL_STATE_MAXIMIZED)
            maximize.set_label("Unmaximize");
        else
            maximize.set_label("Maximize");
    }

    void set_flat_class(bool on)
    {
        if (on) {
            button.get_style_context()->add_class("flat");
        } else {
            button.get_style_context()->remove_class("flat");
        }
    }

    void set_state(uint32_t state)
    {
        this->state = state;
        set_flat_class(!(state & WF_TOPLEVEL_STATE_ACTIVATED));
        update_menu_item_text();
    }

    ~impl()
    {
        zwlr_foreign_toplevel_handle_v1_destroy(handle);
    }


    void handle_output_enter(wl_output *output)
    {
        if (this->parent)
        {
            return;
        }
        auto& container = window_list->box;
        if (window_list->output->wo == output)
        {
            container.add(button);
            container.show_all();
        }

        update_menu_item_text();
    }

    void handle_output_leave(wl_output *output)
    {
        auto& container = window_list->box;
        if (window_list->output->wo == output)
            container.remove(button);
    }
};


WayfireToplevel::WayfireToplevel(WayfireWindowList *window_list,
    zwlr_foreign_toplevel_handle_v1 *handle)
    :pimpl(new WayfireToplevel::impl(window_list, handle)) { }

void WayfireToplevel::set_width(int pixels) { return pimpl->set_max_width(pixels); }
std::vector<zwlr_foreign_toplevel_handle_v1 *>& WayfireToplevel::get_children() { return pimpl->get_children(); }
zwlr_foreign_toplevel_handle_v1 * WayfireToplevel::get_parent() { return pimpl->get_parent(); }
void WayfireToplevel::set_parent(zwlr_foreign_toplevel_handle_v1 *parent) { return pimpl->set_parent(parent); }
uint32_t WayfireToplevel::get_state() { return pimpl->get_state(); }
WayfireToplevel::~WayfireToplevel() = default;

using toplevel_t = zwlr_foreign_toplevel_handle_v1*;
static void handle_toplevel_title(void *data, toplevel_t, const char *title)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->set_title(title);
}

static void handle_toplevel_app_id(void *data, toplevel_t, const char *app_id)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->set_app_id(app_id);
}

static void handle_toplevel_output_enter(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->handle_output_enter(output);
}

static void handle_toplevel_output_leave(void *data, toplevel_t, wl_output *output)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->handle_output_leave(output);
}

/* wl_array_for_each isn't supported in C++, so we have to manually
 * get the data from wl_array, see:
 *
 * https://gitlab.freedesktop.org/wayland/wayland/issues/34 */
template<class T>
static void array_for_each(wl_array *array, std::function<void(T)> func)
{
    assert(array->size % sizeof(T) == 0); // do not use malformed arrays
    for (T* entry = (T*)array->data; (char*)entry < ((char*)array->data + array->size); entry++)
    {
        func(*entry);
    }
}

static void handle_toplevel_state(void *data, toplevel_t, wl_array *state)
{
    uint32_t flags = 0;
    array_for_each<uint32_t> (state, [&flags] (uint32_t st)
    {
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            flags |= WF_TOPLEVEL_STATE_ACTIVATED;
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
            flags |= WF_TOPLEVEL_STATE_MAXIMIZED;
        if (st == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            flags |= WF_TOPLEVEL_STATE_MINIMIZED;
    });

    auto impl = static_cast<WayfireToplevel::impl*> (data);
    impl->set_state(flags);
}

static void handle_toplevel_done(void *data, toplevel_t)
{
//    auto impl = static_cast<WayfireToplevel::impl*> (data);
}

static void remove_child_from_parent(WayfireToplevel::impl *impl, toplevel_t child)
{
    auto parent = impl->get_parent();
    auto& parent_toplevel = impl->window_list->toplevels[parent];
    if (child && parent && parent_toplevel)
    {
        auto& children = parent_toplevel->get_children();
        children.erase(std::find(children.begin(), children.end(), child));
    }
}

static void handle_toplevel_closed(void *data, toplevel_t handle)
{
    //WayfirePanelApp::get().handle_toplevel_closed(handle);
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    remove_child_from_parent(impl, handle);
    impl->window_list->handle_toplevel_closed(handle);
}

static void handle_toplevel_parent(void *data, toplevel_t handle, toplevel_t parent)
{
    auto impl = static_cast<WayfireToplevel::impl*> (data);
    if (!parent)
    {
        if (impl->get_parent())
        {
            impl->handle_output_enter(impl->window_list->output->wo);
        }
        remove_child_from_parent(impl, handle);
        impl->set_parent(parent);
        return;
    }
    if (impl->window_list->toplevels[parent])
    {
        auto& children = impl->window_list->toplevels[parent]->get_children();
        children.push_back(handle);
    }
    impl->set_parent(parent);
    impl->remove_button();
}

namespace
{
struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_v1_impl = {
    .title        = handle_toplevel_title,
    .app_id       = handle_toplevel_app_id,
    .output_enter = handle_toplevel_output_enter,
    .output_leave = handle_toplevel_output_leave,
    .state        = handle_toplevel_state,
    .done         = handle_toplevel_done,
    .closed       = handle_toplevel_closed,
    .parent       = handle_toplevel_parent
};
}

/* Icon loading functions */
namespace IconProvider
{
    using Icon = Glib::RefPtr<Gio::Icon>;

    namespace
    {
        std::string tolower(std::string str)
        {
            for (auto& c : str)
                c = std::tolower(c);
            return str;
        }
    }

    /* Gio::DesktopAppInfo
     *
     * Usually knowing the app_id, we can get a desktop app info from Gio
     * The filename is either the app_id + ".desktop" or lower_app_id + ".desktop" */
    Icon get_from_desktop_app_info(std::string app_id)
    {
        Glib::RefPtr<Gio::DesktopAppInfo> app_info;

        std::vector<std::string> prefixes = {
            "",
            "/usr/share/applications/",
            "/usr/share/applications/kde/",
            "/usr/share/applications/org.kde.",
            "/usr/local/share/applications/",
            "/usr/local/share/applications/org.kde.",
        };

        std::vector<std::string> app_id_variations = {
            app_id,
            tolower(app_id),
        };

        std::vector<std::string> suffixes = {
            "",
            ".desktop"
        };

        for (auto& prefix : prefixes)
        {
            for (auto& id : app_id_variations)
            {
                for (auto& suffix : suffixes)
                {
                    if (!app_info)
                    {
                        app_info = Gio::DesktopAppInfo
                            ::create_from_filename(prefix + id + suffix);
                    }
                }
            }
        }

        if (app_info) // success
            return app_info->get_icon();

        return Icon{};
    }

    /* Second method: Just look up the built-in icon theme,
     * perhaps some icon can be found there */

    void set_image_from_icon(Gtk::Image& image,
        std::string app_id_list, int size, int scale)
    {
        std::string app_id;
        std::istringstream stream(app_id_list);

        /* Wayfire sends a list of app-id's in space separated format, other compositors
         * send a single app-id, but in any case this works fine */
        while (stream >> app_id)
        {
            auto icon = get_from_desktop_app_info(app_id);
            std::string icon_name = "unknown";

            if (!icon)
            {
                /* Perhaps no desktop app info, but we might still be able to
                 * get an icon directly from the icon theme */
                if (Gtk::IconTheme::get_default()->lookup_icon(app_id, 24))
                    icon_name = app_id;
            } else
            {
                icon_name = icon->to_string();
            }

            WfIconLoadOptions options;
            options.user_scale = scale;
            set_image_icon(image, icon_name, size, options);

            /* finally found some icon */
            if (icon_name != "unknown")
                break;
        }
    }
};
