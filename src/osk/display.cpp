#include "display.hpp"
#include "gdk/gdk.h"
#include "gdk/wayland/gdkwayland.h"
#include <cstring>
#include <iostream>

static void registry_add_object(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version)
{
    auto display = static_cast<WaylandDisplay*>(data);
    if (strcmp(interface, zwf_shell_manager_v2_interface.name) == 0)
    {
        display->zwf_manager =
            (zwf_shell_manager_v2*)wl_registry_bind(registry, name,
                &zwf_shell_manager_v2_interface, std::min(version, 1u));
    }

    if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0)
    {
        display->vk_manager = (zwp_virtual_keyboard_manager_v1*)
            wl_registry_bind(registry, name,
            &zwp_virtual_keyboard_manager_v1_interface, 1u);
    }

    if (strcmp(interface, zwp_input_method_manager_v2_interface.name) == 0)
    {
        display->im_manager = (zwp_input_method_manager_v2*)wl_registry_bind(
            registry, name, &zwp_input_method_manager_v2_interface, 1u);
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name)
{
    /* no-op */
}

static struct wl_registry_listener registry_listener =
{
    &registry_add_object,
    &registry_remove_object
};

WaylandDisplay::WaylandDisplay()
{
    auto gdk_display = gdk_display_get_default();
    display = gdk_wayland_display_get_wl_display(gdk_display);

    if (!display)
    {
        std::cerr << "Failed to connect to wayland display!" <<
            " Are you sure you are running a wayland compositor?" << std::endl;
        std::exit(-1);
    }

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (!vk_manager)
    {
        std::cerr << "Compositor doesn't support the virtual-keyboard-v1 " <<
            "protocol, exiting" << std::endl;
        std::exit(-1);
    }
}

WaylandDisplay& WaylandDisplay::get()
{
    static WaylandDisplay instance;
    return instance;
}
