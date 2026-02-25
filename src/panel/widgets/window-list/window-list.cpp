#include <iostream>
#include <glibmm.h>
#include <gdk/wayland/gdkwayland.h>

#include "window-list.hpp"

#define DEFAULT_SIZE_PC 0.1

static void handle_manager_toplevel(void *data, zwlr_foreign_toplevel_manager_v1 *manager,
    zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    std::cout << __func__ << std::endl;
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
    std::cout << __func__ << ": " << interface << std::endl;
    WayfireWindowList *window_list = (WayfireWindowList*)data;

    window_list->foreign_toplevel_manager_id = name;
    window_list->foreign_toplevel_version    = version;

    if (strcmp(interface, wl_output_interface.name) == 0)
    {
        std::cout << "New wl_output: " << name << std::endl;
        wl_output *output = (wl_output*)wl_registry_bind(registry, name, &wl_output_interface, version);
        window_list->handle_new_wl_output(data, registry, name, interface, version, output);
    } else if (strcmp(interface, wl_shm_interface.name) == 0)
    {
        window_list->shm = (wl_shm*)wl_registry_bind(registry, name, &wl_shm_interface, version);
    } else if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
    {
        auto zwlr_toplevel_manager = (zwlr_foreign_toplevel_manager_v1*)
            wl_registry_bind(registry, name,
            &zwlr_foreign_toplevel_manager_v1_interface,
            version);
        std::cout << __func__ << ": " << interface << std::endl;
        window_list->handle_toplevel_manager(zwlr_toplevel_manager);
        zwlr_foreign_toplevel_manager_v1_add_listener(window_list->manager,
            &toplevel_manager_v1_impl, window_list);
        wl_display_roundtrip(window_list->display);
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0)
    {
        window_list->screencopy_manager = (zwlr_screencopy_manager_v1*)wl_registry_bind(registry, name,
            &zwlr_screencopy_manager_v1_interface,
            version);
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{
    std::cout << __func__ << ": " << name << std::endl;
}

void WayfireWindowList::wl_output_enter()
{
    std::cout << __func__ << ": " << " output name: " << this->wayfire_window_list_output->name << std::endl;
    // self.set_live_preview_output(None)
    // if name.startswith("live-preview"):
    // self.set_live_preview_output(output)
    // return
    // if name.startswith("live-preview"):
    // return
    // if not self.my_output and name == self.get_root().monitor_name:
    // self.wl_surface = None
    // if self.get_root().get_native().get_surface():
    // wl_surface_ptr = gtk.gdk_wayland_surface_get_wl_surface(
    // ffi.cast("void *", ctypes.pythonapi.PyCapsule_GetPointer(
    // self.get_root().get_native().get_surface().__gpointer__, None)))
    // self.wl_surface = WlSurface()
    // self.wl_surface._ptr = wl_surface_ptr
    // self.my_output = output
    // else:
    // toplevel_buttons = self.toplevel_buttons.copy()
    // for handle in toplevel_buttons:
    // button = self.toplevel_buttons[handle]
    // if output != button.output:
    // self.foreign_toplevel_closed(handle)
}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

void handle_output_geometry(void*,
    struct wl_output*,
    int32_t,
    int32_t,
    int32_t,
    int32_t,
    int32_t,
    const char*,
    const char*,
    int32_t)
{}

void handle_output_mode(void*,
    struct wl_output*,
    uint32_t,
    int32_t,
    int32_t,
    int32_t)
{}

void handle_output_done(void*, struct wl_output*)
{}

void handle_output_scale(void*, struct wl_output*, int32_t)
{}

void handle_output_name(void *data,
    struct wl_output *wl_output,
    const char *name)
{
    std::cout << __func__ << ": " << data << std::endl;
    std::unique_ptr<WayfireWindowListOutput> *wayfire_window_list_output =
        (std::unique_ptr<WayfireWindowListOutput>*)data;
    (*wayfire_window_list_output)->name = std::string(name);
}

void handle_output_description(void*, struct wl_output*, const char*)
{}

static struct wl_output_listener output_listener =
{
    handle_output_geometry,
    handle_output_mode,
    handle_output_done,
    handle_output_scale,
    handle_output_name,
    handle_output_description,
};

void WayfireWindowList::handle_new_wl_output(void *data, wl_registry *registry, uint32_t name,
    const char *interface, uint32_t version, wl_output *output)
{
    std::cout << __func__ << std::endl;
    this->wayfire_window_list_output = std::make_unique<WayfireWindowListOutput>();
    this->wayfire_window_list_output->output = output;
    this->wayfire_window_list_output->name.clear();
    wl_output_add_listener(output, &output_listener, &this->wayfire_window_list_output);
    // while (this->wayfire_window_list_output->name.empty())
    // {
    // wl_display_dispatch(display);
    // }
    this->wl_output_enter();
    // if not output.name.startswith("live-preview"):
    // if self.foreign_toplevel_manager:
    // self.foreign_toplevel_manager.stop()
    // self.wl_display.roundtrip()
    // if self.foreign_toplevel_manager:
    // self.destroy_foreign_toplevel_manager()
    // self.foreign_toplevel_manager = self.registry.bind(self.foreign_toplevel_manager_id,
    // ZwlrForeignToplevelManagerV1, self.foreign_toplevel_version)
    // self.foreign_toplevel_manager.id = self.foreign_toplevel_manager_id
    // self.wl_globals.append(self.foreign_toplevel_manager)
    // self.foreign_toplevel_manager.dispatcher["toplevel"] = self.on_new_toplevel
    // self.foreign_toplevel_manager.dispatcher["finished"] = self.on_foreign_toplevel_manager_finish
    // self.wl_display.roundtrip()
    // print(f"Monitor {output.name} ({interface}) connected", file=sys.stderr)
}

void WayfireWindowList::init(Gtk::Box *container)
{
    std::cout << __func__ << std::endl;
    auto gdk_display = gdk_display_get_default();
    auto display     = gdk_wayland_display_get_wl_display(gdk_display);

    this->display = display;

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);

    while (!this->manager || this->wayfire_window_list_output->name.empty())
    {
        std::cout << "looping" << std::endl;
        wl_display_dispatch(display);
    }

    std::cout << "output name: " << this->wayfire_window_list_output->name << std::endl;
    if (!this->manager)
    {
        std::cerr << "Compositor doesn't support" <<
            " wlr-foreign-toplevel-management." <<
            "The window-list widget will not be initialized." << std::endl;
        wl_registry_destroy(registry);
        return;
    }

    scrolled_window.add_css_class("window-list");

    // wl_registry_destroy(registry);

    scrolled_window.set_hexpand(true);
    scrolled_window.set_child(*this);
    scrolled_window.set_propagate_natural_width(true);
    scrolled_window.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::NEVER);
    container->append(scrolled_window);
}

void WayfireWindowList::set_top_widget(Gtk::Widget *top)
{
    std::cout << __func__ << std::endl;
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
    std::cout << __func__ << std::endl;
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
    std::cout << __func__ << std::endl;
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
    std::cout << __func__ << std::endl;
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
    std::cout << __func__ << std::endl;
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

void WayfireWindowList::handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager)
{
    std::cout << __func__ << std::endl;
    this->manager = manager;
}

void WayfireWindowList::handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle)
{
    std::cout << __func__ << std::endl;
    toplevels[handle] = std::unique_ptr<WayfireToplevel>(new WayfireToplevel(this, handle));
}

void WayfireWindowList::handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle)
{
    std::cout << __func__ << std::endl;
    toplevels.erase(handle);
}

void WayfireWindowList::on_event(wf::json_t data)
{}

WayfireWindowList::WayfireWindowList(WayfireOutput *output)
{
    std::cout << "WayfireWindowList::WayfireWindowList" << std::endl;
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
    std::cout << "WayfireWindowList::~WayfireWindowList" << std::endl;
    zwlr_foreign_toplevel_manager_v1_destroy(manager);
}
