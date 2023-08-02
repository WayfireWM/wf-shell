#include <iostream>
#include <memory>
#include <glibmm.h>
#include <gdk/gdkwayland.h>

#include "toplevel.hpp"
#include "window-list.hpp"
#include "panel.hpp"

WayfireWindowListBox::WayfireWindowListBox() = default;

void WayfireWindowListBox::set_top_widget(Gtk::Widget *top)
{
    this->top_widget = top;

    if (top_widget)
    {
        /* Set original top_x to where the widget currently is, so that we don't
         * mess with it before the real position is set */
        this->top_x = get_absolute_position(0, *top);
    }

    set_top_x(top_x);
}

void WayfireWindowListBox::set_top_x(int x)
{
    /* Make sure that the widget doesn't go outside of the box */
    if (this->top_widget)
        x = std::min(x, get_allocated_width() - top_widget->get_allocated_width());
    if (this->top_widget)
        x = std::max(x, 0);

    this->top_x = x;

    queue_allocate();
    queue_draw();

    auto alloc = this->get_allocation();
    on_size_allocate(alloc);
}

static void for_each_child_callback(GtkWidget *widget, gpointer data)
{
    auto v = (std::vector<GtkWidget*>*) data;
    v->push_back(widget);
}

std::vector<Gtk::Widget*> WayfireWindowListBox::get_unsorted_widgets()
{
    std::vector<GtkWidget*> children;
    HBox::forall_vfunc(true, &for_each_child_callback, &children);

    std::vector<Gtk::Widget*> result;
    for (auto& child : children)
        result.push_back(Glib::wrap(child));

    return result;
}

void WayfireWindowListBox::forall_vfunc(gboolean value, GtkCallback callback, gpointer callback_data)
{
    std::vector<GtkWidget*> children;
    HBox::forall_vfunc(true, &for_each_child_callback, &children);

    if (top_widget)
    {
        auto it = std::find(children.begin(), children.end(), top_widget->gobj());
        children.erase(it);
        children.push_back(top_widget->gobj());
    }

    for (auto& child : children)
        callback(child, callback_data);
}

void WayfireWindowListBox::on_size_allocate(Gtk::Allocation& alloc)
{
    HBox::on_size_allocate(alloc);

    if (top_widget)
    {
        auto alloc = top_widget->get_allocation();
        alloc.set_x(this->top_x);
        top_widget->size_allocate(alloc);
    }
}

int WayfireWindowListBox::get_absolute_position(int x, Gtk::Widget& ref)
{
    auto w = &ref;
    while (w && w != this)
    {
        auto allocation = w->get_allocation();
        x += allocation.get_x();
        w = w->get_parent();
    }

    return x;
}

Gtk::Widget* WayfireWindowListBox::get_widget_at(int x)
{
    Gtk::Allocation given_point{x, get_allocated_height() / 2, 1, 1};

    /* Widgets are stored bottom to top, so we will return the bottom-most
     * widget at the given position */
    auto children = this->get_children();
    for (auto& child : children)
    {
        if (child->get_allocation().intersects(given_point))
            return child;
    }

    return nullptr;
}

static void handle_manager_toplevel(void *data, zwlr_foreign_toplevel_manager_v1 *manager,
    zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    WayfireWindowList *window_list = (WayfireWindowList *) data;
    window_list->handle_new_toplevel(toplevel);
}

static void handle_manager_finished(void *data, zwlr_foreign_toplevel_manager_v1 *manager)
{
}

zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_v1_impl = {
    .toplevel = handle_manager_toplevel,
    .finished = handle_manager_finished,
};

static void registry_add_object(void *data, wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    WayfireWindowList *window_list = (WayfireWindowList *) data;
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
    {
        auto zwlr_toplevel_manager = (zwlr_foreign_toplevel_manager_v1*)
            wl_registry_bind(registry, name,
                &zwlr_foreign_toplevel_manager_v1_interface,
                std::min(version, 3u));

        window_list->handle_toplevel_manager(zwlr_toplevel_manager);
    }
}
static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name) { }

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

void WayfireWindowList::init(Gtk::HBox *container)
{
    auto gdk_display = gdk_display_get_default();
    auto display = gdk_wayland_display_get_wl_display(gdk_display);

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

    wl_registry_destroy(registry);
    zwlr_foreign_toplevel_manager_v1_add_listener(manager,
        &toplevel_manager_v1_impl, this);

    box.set_homogeneous(true);
    box.show_all();
    container->pack_start(box, true, true);
}

void WayfireWindowList::handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager)
{
    this->manager = manager;
}

void WayfireWindowList::handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle)
{
    toplevels[handle] = std::make_unique<WayfireToplevel>(this, handle);
}

void WayfireWindowList::handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle)
{
    toplevels.erase(handle);
}

WayfireWindowList::WayfireWindowList(WayfireOutput *output)
{
    this->output = output;
}

WayfireWindowList::~WayfireWindowList()
{
    zwlr_foreign_toplevel_manager_v1_destroy(manager);
}
