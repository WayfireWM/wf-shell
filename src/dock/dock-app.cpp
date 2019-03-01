#include "dock.hpp"
#include "toplevel.hpp"
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

class WfDockApp::impl : public WayfireShellApp
{
    std::map<zwlr_foreign_toplevel_handle_v1*,
        std::unique_ptr<WfToplevel>> toplevels;
    std::map<WayfireOutput*, std::unique_ptr<WfDock>> docks;

    zwlr_foreign_toplevel_manager_v1 *manager = NULL;

    public:
    impl(int argc, char **argv)
        :WayfireShellApp(argc, argv)
    {
    }

    void on_activate() override
    {
        WayfireShellApp::on_activate();

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
        zwlr_foreign_toplevel_manager_v1_add_listener(manager,
            &toplevel_manager_v1_impl, NULL);
    }

    void handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager)
    {
        this->manager = manager;
    }

    void on_new_output(WayfireOutput *output) override
    {
        docks[output] = std::unique_ptr<WfDock>(new WfDock(output));
    }

    void on_output_removed(WayfireOutput *output) override
    {
        /* Send an artificial output leave.
         * This is useful because in this way the toplevel can safely destroy
         * its icons created on that particular output */
        for (auto& toplvl : toplevels)
            toplvl.second->handle_output_leave(output->handle);

        docks.erase(output);
    }

    WfDock* dock_for_wl_output(wl_output *output)
    {
        for (auto& dock : docks)
        {
            if (dock.first->handle == output)
                return dock.second.get();
        }

        return nullptr;
    }

    void handle_new_toplevel(zwlr_foreign_toplevel_handle_v1 *handle)
    {
        toplevels[handle] = std::unique_ptr<WfToplevel> (new WfToplevel(handle));
    }

    void handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle)
    {
        toplevels.erase(handle);
    }
};

WfDock* WfDockApp::dock_for_wl_output(wl_output *output)
{
    return pimpl->dock_for_wl_output(output);
}

WayfireDisplay* WfDockApp::get_display()
{
    return pimpl->display.get();
}

wayfire_config* WfDockApp::get_config()
{
    return pimpl->config.get();
}

void WfDockApp::handle_toplevel_manager(zwlr_foreign_toplevel_manager_v1 *manager)
{
    return pimpl->handle_toplevel_manager(manager);
}

void WfDockApp::handle_new_toplevel(zwlr_foreign_toplevel_handle_v1* handle)
{
    return pimpl->handle_new_toplevel(handle);
}

void WfDockApp::handle_toplevel_closed(zwlr_foreign_toplevel_handle_v1 *handle)
{
    return pimpl->handle_toplevel_closed(handle);
}

std::unique_ptr<WfDockApp> WfDockApp::instance;
WfDockApp& WfDockApp::get()
{
    if (!instance)
        throw std::logic_error("Calling WfDockApp::get() before starting app!");
    return *instance.get();
}

void WfDockApp::run(int argc, char **argv)
{
    if (instance)
        throw std::logic_error("Running WfDockApp twice!");

    instance = std::unique_ptr<WfDockApp>{new WfDockApp(argc, argv)};
    instance->pimpl->run();
}

WfDockApp::WfDockApp(int argc, char **argv)
    : pimpl(new WfDockApp::impl(argc, argv)) { }
WfDockApp::~WfDockApp() = default;

int main(int argc, char **argv)
{
    WfDockApp::run(argc, argv);
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
