#include "display.hpp"
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>
#include <iostream>
#include <cstring>

// listeners
static void registry_add_object(void *data, struct wl_registry *registry, uint32_t name,
        const char *interface, uint32_t version)
{
    auto display = static_cast<WayfireDisplay*> (data);
    if (strcmp(interface, zwf_shell_manager_v1_interface.name) == 0)
    {
        display->zwf_shell_manager =
            (zwf_shell_manager_v1*) wl_registry_bind(registry, name,
                                                     &zwf_shell_manager_v1_interface,
                                                     std::min(version, 1u));
    }

    if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0)
    {
        display->zxdg_output_manager = (zxdg_output_manager_v1*)
            wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface,
                             std::min(version, 2u));
    }

    if (strcmp(interface, wl_output_interface.name) == 0)
    {
        auto output = (wl_output*) wl_registry_bind(registry, name, &wl_output_interface,
                                                    std::min(version, 1u));
        display->name_to_wayfire_output[name] = new WayfireOutput(display, output);
    }

    if (strcmp(interface, wl_seat_interface.name) == 0 && !display->default_seat)
    {
        display->default_seat = (wl_seat*) wl_registry_bind(registry, name,
            &wl_seat_interface, std::min(version, 1u));
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{
    auto display = (WayfireDisplay*) data;
    if (display->name_to_wayfire_output.count(name))
    {
        delete display->name_to_wayfire_output[name];
        display->name_to_wayfire_output.erase(name);
    }
}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

/* WayfireDisplay implementation */
WayfireDisplay::WayfireDisplay(std::function<void(WayfireOutput*)> new_output_cb)
{
    this->new_output_callback = new_output_cb;

    auto gdk_display = gdk_display_get_default();
    auto display = gdk_wayland_display_get_wl_display(gdk_display);

    if (!display)
    {
        std::cerr << "Failed to connect to wayland display!"
            << " Are you sure you are running a wayland compositor?" << std::endl;
        std::exit(-1);
    }

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);

    if (!this->zxdg_output_manager)
    {
        std::cerr << "xdg_output not available" << std::endl;
        std::exit(-1);
    }

    if (!this->zwf_shell_manager)
    {
        std::cerr << "wayfire-shell not available" << std::endl;
        std::exit(-1);
    }
}

WayfireDisplay::~WayfireDisplay()
{
    zxdg_output_manager_v1_destroy(zxdg_output_manager);

    // TODO: we should also destroy all kinds of shells,
    // registry, etc. here
    wl_display_disconnect(display);
}

/* zxdg_output impl */
static void zxdg_output_logical_position(void *data, struct zxdg_output_v1 *zxdg_output_v1,
                                         int32_t x, int32_t y)
{ }

static void zxdg_output_logical_size(void *data, struct zxdg_output_v1 *zxdg_output_v1,
                                     int32_t width, int32_t height)
{
    auto wo = (WayfireOutput*) data;
    if (wo->resized_callback)
        wo->resized_callback(wo, width, height);
}

static void zxdg_output_done(void *data, struct zxdg_output_v1 *zxdg_output_v1) { }
static void zxdg_output_name(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *name) {}
static void zxdg_output_description(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *description) {}

const struct zxdg_output_v1_listener zxdg_output_v1_impl =
{
    zxdg_output_logical_position,
    zxdg_output_logical_size,
    zxdg_output_done,
    zxdg_output_name,
    zxdg_output_description
};

/* WayfireOutput implementation */
WayfireOutput::WayfireOutput(WayfireDisplay *display, wl_output *output)
{
    this->display = display;
    this->handle = output;

    zxdg_output = zxdg_output_manager_v1_get_xdg_output(display->zxdg_output_manager, handle);
    zxdg_output_v1_add_listener(zxdg_output, &zxdg_output_v1_impl, this);

    if (display->zwf_shell_manager)
        zwf = zwf_shell_manager_v1_get_wf_output(display->zwf_shell_manager, output);

    if (display->new_output_callback)
        display->new_output_callback(this);
}

WayfireOutput::~WayfireOutput()
{
    if (destroyed_callback)
        destroyed_callback(this);

    zxdg_output_v1_destroy(zxdg_output);
}
