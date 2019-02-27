#include <iostream>
#include <glibmm.h>
#include <gdk/gdkwayland.h>

#include "toplevel.hpp"
#include "window-list.hpp"
#include "panel.hpp"
#include "config.hpp"

static void handle_manager_toplevel(void *data, zwlr_foreign_toplevel_manager_v1 *manager,
    zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    WayfireWindowList *window_list = (WayfireWindowList *) data;
    window_list->handle_new_toplevel(toplevel);
}

static void handle_manager_finished(void *data, zwlr_foreign_toplevel_manager_v1 *manager)
{
    /* TODO: maybe exit? */
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
                std::min(version, 1u));

        window_list->handle_toplevel_manager(zwlr_toplevel_manager);
    }
}
static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name) { }

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

void WayfireWindowList::init(Gtk::HBox *container, wayfire_config *config)
{
    auto gdk_display = gdk_display_get_default();
    auto display = gdk_wayland_display_get_wl_display(gdk_display);

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);

    if (!this->manager)
    {
        std::cerr << "Compositor doesn't support" <<
            " wlr-foreign-toplevel-management, exiting." << std::endl;
        std::exit(-1);
    }

    wl_registry_destroy(registry);
    zwlr_foreign_toplevel_manager_v1_add_listener(manager,
        &toplevel_manager_v1_impl, this);

    scrolled_window.add(box);
    scrolled_window.set_hexpand(true);
    scrolled_window.set_halign(Gtk::ALIGN_FILL);
    //scrolled_window.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_NEVER);
    container->pack_start(scrolled_window, Gtk::PACK_EXPAND_WIDGET, 0);
    scrolled_window.show_all();
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
    toplevels[handle] = std::unique_ptr<WayfireToplevel> (new WayfireToplevel(this, handle, box));
}

void WayfireWindowList::handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle)
{
    toplevels.erase(handle);
}

WayfireWindowList::WayfireWindowList()
{
}

WayfireWindowList::~WayfireWindowList()
{
}
