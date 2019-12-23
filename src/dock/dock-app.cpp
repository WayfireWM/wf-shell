#include "dock.hpp"
#include "toplevel.hpp"
#include "toplevel-icon.hpp"
#include <iostream>
#include <gdk/gdkwayland.h>


namespace {
    extern zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_v1_impl;
}

static void registry_add_object(void *data, wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
    {
        auto zwlr_toplevel_manager = (zwlr_foreign_toplevel_manager_v1*)
            wl_registry_bind(registry, name,
                &zwlr_foreign_toplevel_manager_v1_interface,
                std::min(version, 1u));

        WfDockApp::get().handle_toplevel_manager(zwlr_toplevel_manager);
    }
}
static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name) { }

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

class WfDockApp::impl
{
  public:
    std::map<zwlr_foreign_toplevel_handle_v1*,
        std::unique_ptr<WfToplevel>> toplevels;
    std::map<WayfireOutput*, std::unique_ptr<WfDock>> docks;

    zwlr_foreign_toplevel_manager_v1 *manager = NULL;
};

void WfDockApp::on_activate()
{
    WayfireShellApp::on_activate();
    IconProvider::load_custom_icons();

    /* At this point, wayland connection has been initialized,
     * and hopefully outputs have been created */
    auto gdk_display = gdk_display_get_default();
    auto display = gdk_wayland_display_get_wl_display(gdk_display);

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!this->manager)
    {
        std::cerr << "Compositor doesn't support" <<
            " wlr-foreign-toplevel-management, exiting." << std::endl;
        std::exit(-1);
    }

    wl_registry_destroy(registry);
    zwlr_foreign_toplevel_manager_v1_add_listener(priv->manager,
        &toplevel_manager_v1_impl, NULL);
}

void WfDockApp::handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager)
{
    priv->manager = manager;
}

void WfDockApp::handle_new_output(WayfireOutput *output)
{
    priv->docks[output] = std::unique_ptr<WfDock>(new WfDock(output));
}

void WfDockApp::handle_output_removed(WayfireOutput *output)
{
    /* Send an artificial output leave.
     * This is useful because in this way the toplevel can safely destroy
     * its icons created on that particular output */
    for (auto& toplvl : priv->toplevels)
        toplvl.second->handle_output_leave(output->wo);

    priv->docks.erase(output);
}

WfDock* WfDockApp::dock_for_wl_output(wl_output *output)
{
    for (auto& dock : priv->docks)
    {
        if (dock.first->wo == output)
            return dock.second.get();
    }

    return nullptr;
}

void WfDockApp::handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle)
{
    priv->toplevels[handle] =
        std::unique_ptr<WfToplevel> (new WfToplevel(handle));
}

void WfDockApp::handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle)
{
    priv->toplevels.erase(handle);
}

WfDockApp& WfDockApp::get()
{
    if (!instance)
        throw std::logic_error("Calling WfDockApp::get() before starting app!");

    return dynamic_cast<WfDockApp&> (*instance.get());
}

void WfDockApp::create(int argc, char **argv)
{
    if (instance)
        throw std::logic_error("Running WfDockApp twice!");

    instance = std::unique_ptr<WfDockApp>{new WfDockApp(argc, argv)};
    instance->run();
}

WfDockApp::WfDockApp(int argc, char **argv)
    : WayfireShellApp(argc, argv), priv(new WfDockApp::impl()) { }
WfDockApp::~WfDockApp() = default;

int main(int argc, char **argv)
{
    WfDockApp::create(argc, argv);
    return 0;
}

using manager_v1_t = zwlr_foreign_toplevel_manager_v1;
static void handle_manager_toplevel(void *data, manager_v1_t *manager,
    zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    WfDockApp::get().handle_new_toplevel(toplevel);
}

static void handle_manager_finished(void *data, manager_v1_t *manager)
{
    /* TODO: maybe exit? */
}

namespace{
zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_v1_impl = {
    .toplevel = handle_manager_toplevel,
    .finished = handle_manager_finished,
};
}
