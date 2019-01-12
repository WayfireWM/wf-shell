#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <wayland-client.h>
#include <functional>
#include <map>

#include "xdg-output-unstable-v1-client-protocol.h"
#include "wayfire-shell-client-protocol.h"

struct WayfireOutput;
using output_callback = std::function<void(WayfireOutput*)>;
struct WayfireDisplay
{
    wl_display    *display = nullptr;
    wl_seat       *default_seat = nullptr;

    zwf_shell_manager_v1   *zwf_shell_manager = nullptr;
    zxdg_output_manager_v1 *zxdg_output_manager = nullptr;

    WayfireDisplay(output_callback new_output_cb);
    ~WayfireDisplay();

    std::map<uint32_t, WayfireOutput*> name_to_wayfire_output;
    output_callback new_output_callback;
};

struct WayfireOutput
{
    WayfireDisplay *display;

    wl_output *handle = nullptr;
    zxdg_output_v1 *zxdg_output = nullptr;
    zwf_output_v1 *zwf = nullptr;

    std::function<void(WayfireOutput*)> destroyed_callback;
    std::function<void(WayfireOutput*, int32_t, int32_t)> resized_callback;

    WayfireOutput(WayfireDisplay *display, wl_output *);
    ~WayfireOutput();
};

#endif /* end of include guard: DISPLAY_HPP */
