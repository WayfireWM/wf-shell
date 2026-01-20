#include <iostream>
#include <glibmm.h>
#include <gdk/wayland/gdkwayland.h>

#include "window-list.hpp"

#define DEFAULT_SIZE_PC 0.1

static void handle_manager_toplevel(void *data, zwlr_foreign_toplevel_manager_v1 *manager,
    zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;
    window_list->handle_new_toplevel(toplevel);
}

static void handle_manager_finished(void *data, zwlr_foreign_toplevel_manager_v1 *manager)
{}

zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_v1_impl = {
    .toplevel = handle_manager_toplevel,
    .finished = handle_manager_finished,
};

static void registry_add_object(void *data, wl_registry *registry, uint32_t name,
    const char *interface, uint32_t version)
{
    WayfireWindowList *window_list = (WayfireWindowList*)data;
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
    {
        auto zwlr_toplevel_manager = (zwlr_foreign_toplevel_manager_v1*)
            wl_registry_bind(registry, name,
            &zwlr_foreign_toplevel_manager_v1_interface,
            std::min(version, 3u));

        window_list->handle_toplevel_manager(zwlr_toplevel_manager);
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

void WayfireWindowList::init(Gtk::Box *container)
{
    auto gdk_display = gdk_display_get_default();
    auto display     = gdk_wayland_display_get_wl_display(gdk_display);

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);

    if (!this->manager)
    {
        std::cerr << "Compositor doesn't support" <<
            " wlr-foreign-toplevel-management." <<
            "The window-list widget will not be initialized." << std::endl;
        wl_registry_destroy(registry);
        return;
    }

    scrolled_window.get_style_context()->add_class("window-list");

    wl_registry_destroy(registry);
    zwlr_foreign_toplevel_manager_v1_add_listener(manager,
        &toplevel_manager_v1_impl, this);

    scrolled_window.set_hexpand(true);
    scrolled_window.set_child(*this);
    scrolled_window.set_propagate_natural_width(true);
    scrolled_window.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::NEVER);
    container->append(scrolled_window);
}

void WayfireWindowList::set_top_widget(Gtk::Widget *top)
{
    this->layout->top_widget = top;

    if (layout->top_widget)
    {
        /* Set original top_x to where the widget currently is, so that we don't
         * mess with it before the real position is set */
        this->layout->top_x = get_absolute_position(0, *top);
    }

    set_top_x(layout->top_x);
}

void WayfireWindowList::set_top_x(int x)
{
    /* Make sure that the widget doesn't go outside of the box */
    if (this->layout->top_widget)
    {
        x = std::min(x, get_allocated_width() - layout->top_widget->get_allocated_width());
    }

    if (this->layout->top_widget)
    {
        x = std::max(x, 0);
    }

    this->layout->top_x = x;

    if (this->layout->top_widget)
    {
        // TODO Sensibly cause a reflow to force layout manager to move children
    }

    queue_allocate();
    queue_draw();
}

int WayfireWindowList::get_absolute_position(int x, Gtk::Widget& ref)
{
    auto w = &ref;
    while (w && w != this)
    {
        auto allocation = w->get_allocation();
        x += allocation.get_x();
        w  = w->get_parent();
    }

    return x;
}

Gtk::Widget*WayfireWindowList::get_widget_before(int x)
{
    Gtk::Allocation given_point{x, get_allocated_height() / 2, 1, 1};

    /* Widgets are stored bottom to top, so we will return the bottom-most
     * widget at the given position */
    Gtk::Widget *previous = nullptr;
    auto children = this->get_children();
    for (auto& child : children)
    {
        if (layout->top_widget && (child == layout->top_widget))
        {
            continue;
        }

        if (child->get_allocation().intersects(given_point))
        {
            return previous;
        }

        previous = child;
    }

    return nullptr;
}

Gtk::Widget*WayfireWindowList::get_widget_at(int x)
{
    Gtk::Allocation given_point{x, get_allocated_height() / 2, 1, 1};

    /* Widgets are stored bottom to top, so we will return the bottom-most
     * widget at the given position */
    auto children = this->get_children();
    for (auto& child : children)
    {
        if (child == layout->top_widget)
        {
            continue;
        }

        if (child->get_allocation().intersects(given_point))
        {
            return child;
        }
    }

    return nullptr;
}

void WayfireWindowList::add_output(WayfireOutput *output)
{
    std::unique_ptr<WayfireWindowList>();
}

void WayfireWindowList::handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager)
{
    this->manager = manager;
}

void WayfireWindowList::handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle)
{
    toplevels[handle] = std::unique_ptr<WayfireToplevel>(new WayfireToplevel(this, handle));
}

void WayfireWindowList::handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle)
{
    toplevels.erase(handle);

    /* No size adjustments necessary in this case */
    if (toplevels.size() == 0)
    {
        return;
    }
}

WayfireWindowList::WayfireWindowList(WayfireOutput *output)
{
    this->output = output;

    layout = std::make_shared<WayfireWindowListLayout>(this);
    set_layout_manager(layout);
    user_size.set_callback([=]
    {
        this->queue_allocate();
    });
}

WayfireWindowList::~WayfireWindowList()
{
    zwlr_foreign_toplevel_manager_v1_destroy(manager);
}
