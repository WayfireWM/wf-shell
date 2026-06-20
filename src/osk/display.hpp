#pragma once

#include "input-method-unstable-v2-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"
#include "wayfire-shell-unstable-v2-client-protocol.h"
#include <wayland-client-core.h>
class WaylandDisplay
{
    WaylandDisplay();

  public:
    static WaylandDisplay& get();

    wl_display *display = nullptr;

    zwf_shell_manager_v2 *zwf_manager = nullptr;
    zwp_virtual_keyboard_manager_v1 *vk_manager = nullptr;
    zwp_input_method_manager_v2 *im_manager     = nullptr;
};
