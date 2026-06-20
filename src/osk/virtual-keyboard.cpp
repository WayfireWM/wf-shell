#include "virtual-keyboard.hpp"
#include "gdk/gdk.h"
#include "gdk/wayland/gdkwayland.h"
#include "input-method-unstable-v2-client-protocol.h"
#include "osk.hpp"
#include "wf-ipc.hpp"
#include "wayland-window.hpp"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <cstring>
#include <time.h>
#include <iostream>
#include "display.hpp"

#include <gdkmm/display.h>
#include <gdkmm/seat.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbregistry.h>

uint32_t get_current_time()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ll + ts.tv_nsec / 1000000ll;
}

/* Keyboard callbacks */
static void kbd_keymap(void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t format,
    int32_t fd,
    uint32_t size)
{
    auto instance = static_cast<VirtualKeyboardDevice*>(data);
    instance->handle_keymap(data, format, fd, size);
}

static void kbd_enter(void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    struct wl_surface *surface,
    struct wl_array *keys)
{
    /* never called */
}

static void kbd_leave(void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    struct wl_surface *surface)
{
    /* never called */
}

static void kbd_key(void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    uint32_t time,
    uint32_t key,
    uint32_t state)
{
    /* never called */
}

static void kbd_modifiers(void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    uint32_t mods_depressed,
    uint32_t mods_latched,
    uint32_t mods_locked,
    uint32_t group)
{
    /* never called */
}

static void kbd_repeat_info(void *data,
    struct wl_keyboard *wl_keyboard,
    int32_t rate,
    int32_t delay)
{
    /* Don't care */
}

static const wl_keyboard_listener kbd_listener = {
    kbd_keymap,
    kbd_enter,
    kbd_leave,
    kbd_key,
    kbd_modifiers,
    kbd_repeat_info,
};

/* Input method callbacks */

struct InputMethodState
{
    std::string surrounding_text = "";
    uint32_t cursor_index = 0;
    uint32_t anchor_index = 0;
    uint32_t hint    = 0;
    uint32_t purpose = 0;
    std::string active_word_fragment = "";
    uint32_t serial = 0;
    bool is_active;
};

static InputMethodState im_state;

static std::pair<std::string, std::string> extract_partial_word(const std::string& text, uint32_t cursor)
{
    if ((cursor == 0) || text.empty() || (cursor > text.length()))
    {
        return {"", ""};
    }

    size_t start_pos = text.find_last_of(" \t\n\r", cursor - 1);

    if (start_pos == std::string::npos)
    {
        return {"", text.substr(0, cursor)}; /* Word starts at the very beginning of the buffer */
    }

    return {text.substr(0, start_pos), text.substr(start_pos + 1, cursor - (start_pos + 1))};
}

static void im_activate(void *data,
    struct zwp_input_method_v2 *zwp_input_method_v2)
{
    im_state.is_active = true;
    std::cout << "Activate " << std::endl;
    auto instance = static_cast<VirtualKeyboardDevice*>(data);
    instance->handle_activate();
}

static void im_deactivate(void *data,
    struct zwp_input_method_v2 *zwp_input_method_v2)
{
    im_state.is_active = false;
    std::cout << "Deactivate " << std::endl;
    auto instance = static_cast<VirtualKeyboardDevice*>(data);
    instance->handle_deactivate();
}

static void im_surrounding_text(void *data,
    struct zwp_input_method_v2 *zwp_input_method_v2,
    const char *text,
    uint32_t cursor,
    uint32_t anchor)
{
    if (text)
    {
        im_state.surrounding_text = text;
        im_state.cursor_index     = cursor;
        im_state.anchor_index     = anchor;
    }
}

static void im_text_change_cause(void *data,
    struct zwp_input_method_v2 *zwp_input_method_v2,
    uint32_t cause)
{}

static void im_content_type(void *data,
    struct zwp_input_method_v2 *zwp_input_method_v2,
    uint32_t hint,
    uint32_t purpose)
{
    im_state.hint    = hint;
    im_state.purpose = purpose;
}

static void im_done(void *data,
    struct zwp_input_method_v2 *zwp_input_method_v2)
{
    auto instance = static_cast<VirtualKeyboardDevice*>(data);

    im_state.serial++;

    instance->done();
    std::cout << "done - serial: " << im_state.serial << " surrounding : '" << im_state.surrounding_text <<
        "'" << std::endl;
}

static void im_unavailable(void *data,
    struct zwp_input_method_v2 *zwp_input_method_v2)
{
    std::exit(-1);
}

static const zwp_input_method_v2_listener im_listener = {
    im_activate,
    im_deactivate,
    im_surrounding_text,
    im_text_change_cause,
    im_content_type,
    im_done,
    im_unavailable,
};

VirtualKeyboardDevice::VirtualKeyboardDevice()
{
    auto& display = WaylandDisplay::get();
    auto seat     = Gdk::Display::get_default()->get_default_seat();
    auto wl_seat  = gdk_wayland_seat_get_wl_seat(seat->gobj());

    auto wl_keyboard = wl_seat_get_keyboard(wl_seat);
    wl_keyboard_add_listener(wl_keyboard, &kbd_listener, this);
    vk = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
        display.vk_manager, wl_seat);
    xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    if (display.im_manager)
    {
        im = zwp_input_method_manager_v2_get_input_method(display.im_manager, wl_seat);
        zwp_input_method_v2_add_listener(im, &im_listener, this);
    }

    ipc_server = WayfireIPC::get_instance();
    if (!ipc_server)
    {
        std::cerr << "Failed to connect to Wayfire IPC. exiting" << std::endl;
        std::exit(-1);
    }

    ipc_client = ipc_server->create_client();

    if (!ipc_client)
    {
        std::cerr << "Failed to connect to Wayfire IPC client. exiting" << std::endl;
        std::exit(-1);
    }

    ipc_client->subscribe(this, {"keyboard-modifier-state-changed"});
}

VirtualKeyboardDevice::~VirtualKeyboardDevice()
{
    if (xkb_keymap)
    {
        xkb_keymap_unref(xkb_keymap);
    }

    if (xkb_context)
    {
        xkb_context_unref(xkb_context);
    }
}

void VirtualKeyboardDevice::done()
{
    is_active = im_state.is_active;
    if ((current_hint != im_state.hint) || (current_purpose != im_state.purpose))
    {
        current_hint    = im_state.hint;
        current_purpose = im_state.purpose;
        hints_changed.emit();
    }

    auto [before, partial] = extract_partial_word(im_state.surrounding_text,
        im_state.cursor_index);
    im_state.active_word_fragment = partial;
    if (!is_sensitive())
    {
        WayfireOsk::get().get_complete().get_suggestions(before, partial);
    } else
    {
        WayfireOsk::get().get_window().clear_suggestions();
    }
}

void VirtualKeyboardDevice::on_event(wf::json_t data)
{
    std::cout << data.serialize() << std::endl;
    if (data["event"].as_string() == "keyboard-modifier-state-changed")
    {
        if (available_layouts.size() == 0)
        {
            set_available(data["state"]["possible-layouts"]);
        }

        auto state_layout = data["state"]["layout-index"].as_uint();
        if (state_layout != current_layout)
        {
            current_layout = state_layout;
            set_current(state_layout);
        }
    }
}

void VirtualKeyboardDevice::set_available(wf::json_t layouts)
{
    std::vector<Layout> layouts_available;
    std::map<std::string, uint32_t> names;

    for (size_t i = 0; i < layouts.size(); i++)
    {
        auto elem = layouts[i];
        names[elem] = i;
        layouts_available.push_back(Layout{
            .Name   = (std::string)elem,
            .ID     = "",
            .Locale = "",
        });
    }

    auto context = rxkb_context_new(RXKB_CONTEXT_NO_FLAGS);
    rxkb_context_parse_default_ruleset(context);
    auto rlayout = rxkb_layout_first(context);
    for (; rlayout != NULL; rlayout = rxkb_layout_next(rlayout))
    {
        auto descr = rxkb_layout_get_description(rlayout);
        auto name  = names.find(descr);
        if (name != names.end())
        {
            layouts_available[name->second].ID = rxkb_layout_get_brief(rlayout);

            struct rxkb_iso3166_code *iso3166 = rxkb_layout_get_iso3166_first(rlayout);

            const char *country = iso3166 ? rxkb_iso3166_code_get_code(iso3166) : nullptr;
            if (country)
            {
                layouts_available[name->second].Locale = layouts_available[name->second].ID + "_" + country;
            } else
            {
                layouts_available[name->second].Locale = layouts_available[name->second].ID;
            }
        }
    }

    available_layouts = layouts_available;
}

void VirtualKeyboardDevice::set_current(uint32_t index)
{
    current_layout = index;

    layer_changed.emit();
    WayfireOsk::get().get_complete().switch_language(available_layouts[current_layout].Locale,
        available_layouts[current_layout].Name);
}

void VirtualKeyboardDevice::handle_keymap(void *data, uint32_t format,
    int32_t fd, uint32_t size)
{
    std::cout << "HANDLE KEYMAP" << std::endl;
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    {
        close(fd);
        return;
    }

    bool req_send_keymap = false;

    char *map_str = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
    if (map_str != MAP_FAILED)
    {
        auto next_keymap = std::string(map_str, size);
        if (keymap != next_keymap)
        {
            req_send_keymap = true;
        }

        keymap = next_keymap;
        if (xkb_keymap)
        {
            xkb_keymap_unref(xkb_keymap);
        }

        xkb_keymap = xkb_keymap_new_from_string(xkb_context, map_str,
            XKB_KEYMAP_FORMAT_TEXT_V1,
            XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map_str, size);
        ready_changed.emit(valid());
    }

    close(fd);
    if (req_send_keymap)
    {
        send_keymap();
    }

    keymap_changed.emit();
}

int VirtualKeyboardDevice::create_shm_file(size_t size)
{
    int fd = memfd_create("vk_keymap", MFD_CLOEXEC);
    if (fd < 0)
    {
        return -1;
    }

    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}

void VirtualKeyboardDevice::send_keymap()
{
    if (!vk || keymap.empty())
    {
        return;
    }

    auto chars  = keymap.c_str();
    size_t size = strlen(chars) + 1;
    int fd = create_shm_file(size);
    if (fd < 0)
    {
        return;
    }

    void *data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        close(fd);
        return;
    }

    std::memcpy(data, chars, size);
    munmap(data, size);

    zwp_virtual_keyboard_v1_keymap(vk,
        WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
        fd, size);
    close(fd);

    keymap_changed.emit();
}

void VirtualKeyboardDevice::send_string(std::string message)
{
    bool insert_with_input_method = false;
    if (insert_with_input_method)
    {
        /* Theoretically sound. Breaks often. Just like me */

        if (!is_active)
        {
            std::cout << "Not active, skipped" << std::endl;
            return;
        }

        if (im == nullptr)
        {
            std::cerr << "Cannot send arbitrary string without input_method_v2" << std::endl;
            return;
        }

        std::cout << "Send '" << message << "' " << im_state.serial << std::endl;
        zwp_input_method_v2_delete_surrounding_text(im, 0, 0);
        zwp_input_method_v2_commit_string(im, message.c_str());
        zwp_input_method_v2_commit(im, im_state.serial);
    } else
    {
        /* I love this idea, it's so stupid.
         *
         *  why does it work so well?
         */
        if (!vk || message.empty())
        {
            return;
        }

        std::vector<uint32_t> codepoints = utf8_to_codepoints(message);
        if (codepoints.empty())
        {
            return;
        }

        for (uint32_t codepoint : codepoints)
        {
            std::stringstream ss;
            ss << std::hex << codepoint;
            std::string hex_str = ss.str();

            zwp_virtual_keyboard_v1_modifiers(vk, 5, 0, 0, current_layout);
            send_key_raw(KEY_LEFTCTRL, true);
            send_key_raw(KEY_LEFTSHIFT, true);
            send_key_raw(KEY_U, true);
            send_key_raw(KEY_U, false);
            send_key_raw(KEY_LEFTCTRL, false);
            send_key_raw(KEY_LEFTSHIFT, false);
            zwp_virtual_keyboard_v1_modifiers(vk, 0, 0, 0, current_layout);

            for (char c : hex_str)
            {
                uint32_t kc = hex_char_to_keycode(c);
                if (kc != 0)
                {
                    send_key_raw(kc, true);
                    send_key_raw(kc, false);
                }
            }

            send_key_raw(KEY_ENTER, true);
            send_key_raw(KEY_ENTER, false);
        }
    }
}

std::vector<uint32_t> VirtualKeyboardDevice::utf8_to_codepoints(const std::string& str)
{
    /* Magic */
    std::vector<uint32_t> codepoints;
    codepoints.reserve(str.size());

    size_t i = 0;
    while (i < str.size())
    {
        uint32_t point = 0;

        auto c = static_cast<unsigned char>(str[i]);

        if (c < 0x80)
        {
            point = c;
            i    += 1;
        } else if (((c & 0xE0) == 0xC0) && (i + 1 < str.size()))
        {
            point = ((c & 0x1F) << 6) |
                (static_cast<unsigned char>(str[i + 1]) & 0x3F);
            i += 2;
        } else if (((c & 0xF0) == 0xE0) && (i + 2 < str.size()))
        {
            point = ((c & 0x0F) << 12) |
                ((static_cast<unsigned char>(str[i + 1]) & 0x3F) << 6) |
                (static_cast<unsigned char>(str[i + 2]) & 0x3F);
            i += 3;
        } else if (((c & 0xF8) == 0xF0) && (i + 3 < str.size()))
        {
            point = ((c & 0x07) << 18) |
                ((static_cast<unsigned char>(str[i + 1]) & 0x3F) << 12) |
                ((static_cast<unsigned char>(str[i + 2]) & 0x3F) << 6) |
                (static_cast<unsigned char>(str[i + 3]) & 0x3F);
            i += 4;
        } else
        {
            /* Skip malformed individual UTF-8 bytes gracefully */
            i += 1;
            continue;
        }

        codepoints.push_back(point);
    }

    codepoints.shrink_to_fit();
    return codepoints;
}

/*
 * To emulate Hexcode keys, use scancodes
 * FIXME: Will most likely not function on alternative layouts...
 */
uint32_t VirtualKeyboardDevice::hex_char_to_keycode(char c)
{
    switch (c)
    {
      case '0':
        return KEY_0;

      case '1':
        return KEY_1;

      case '2':
        return KEY_2;

      case '3':
        return KEY_3;

      case '4':
        return KEY_4;

      case '5':
        return KEY_5;

      case '6':
        return KEY_6;

      case '7':
        return KEY_7;

      case '8':
        return KEY_8;

      case '9':
        return KEY_9;

      case 'a':
      case 'A':
        return KEY_A;

      case 'b':
      case 'B':
        return KEY_B;

      case 'c':
      case 'C':
        return KEY_C;

      case 'd':
      case 'D':
        return KEY_D;

      case 'e':
      case 'E':
        return KEY_E;

      case 'f':
      case 'F':
        return KEY_F;

      default:
        return 0;
    }
}

/* Without changing modifiers send keys */
void VirtualKeyboardDevice::send_key_raw(uint32_t key, uint32_t state) const
{
    zwp_virtual_keyboard_v1_key(vk, get_current_time(), key, state);
}

void VirtualKeyboardDevice::send_key(uint32_t key, uint32_t state) const
{
    /* TODO When we can globally track state, don't modify it here */
    if ((state == 1) && (mods_depressed != 0))
    {
        zwp_virtual_keyboard_v1_modifiers(vk,
            mods_depressed,
            0,
            mods_locked,
            current_layout);
    }

    send_key_raw(key, state);
    if ((state == 0) && (mods_depressed != 0))
    {
        zwp_virtual_keyboard_v1_modifiers(vk, 0, 0, 0, current_layout);
    }
}

void VirtualKeyboardDevice::toggle_modifier(std::string mod_name)
{
    if ((mod_name == "Lock") || (mod_name == "Caps") || (mod_name == "CapsLock") ||
        (mod_name == "Mod2") || (mod_name == "NumLock") || (mod_name == "ScrollLock"))
    {
        uint32_t mask_bit = 0;
        if ((mod_name == "Lock") || (mod_name == "Caps") || (mod_name == "CapsLock"))
        {
            mask_bit = (1U << 1);
        } else if ((mod_name == "Mod2") || (mod_name == "NumLock"))
        {
            mask_bit = (1U << 4);
        } else if (mod_name == "ScrollLock")
        {
            mask_bit = (1U << 5);
        }

        if (mask_bit != 0)
        {
            mods_locked ^= mask_bit;
            modifiers_changed.emit();
        }
    } else
    {
        uint32_t mask_bit = 0;
        if (mod_name == "Shift")
        {
            mask_bit = (1U << 0);
        } else if ((mod_name == "Control") || (mod_name == "Ctrl"))
        {
            mask_bit = (1U << 2);
        } else if ((mod_name == "Mod1") || (mod_name == "Alt"))
        {
            mask_bit = (1U << 3);
        } else if ((mod_name == "Mod4") || (mod_name == "Super") || (mod_name == "Logo"))
        {
            mask_bit = (1U << 6);
        }

        if (mask_bit != 0)
        {
            mods_depressed ^= mask_bit;
            modifiers_changed.emit();
        }
    }
}

void VirtualKeyboardDevice::set_modifier(std::string mod_name, bool val)
{
    if ((mod_name == "Lock") || (mod_name == "Caps") || (mod_name == "CapsLock") ||
        (mod_name == "Mod2") || (mod_name == "NumLock") || (mod_name == "ScrollLock"))
    {
        uint32_t mask_bit = 0;
        if ((mod_name == "Lock") || (mod_name == "Caps") || (mod_name == "CapsLock"))
        {
            mask_bit = (1U << 1);
        } else if ((mod_name == "Mod2") || (mod_name == "NumLock"))
        {
            mask_bit = (1U << 4);
        } else if (mod_name == "ScrollLock")
        {
            mask_bit = (1U << 5);
        }

        if (mask_bit != 0)
        {
            mods_locked = (mods_locked & ~mask_bit) | (val ? mask_bit : 0);
            modifiers_changed.emit();
        }
    } else
    {
        uint32_t mask_bit = 0;
        if (mod_name == "Shift")
        {
            mask_bit = (1U << 0);
        } else if ((mod_name == "Control") || (mod_name == "Ctrl"))
        {
            mask_bit = (1U << 2);
        } else if ((mod_name == "Mod1") || (mod_name == "Alt"))
        {
            mask_bit = (1U << 3);
        } else if ((mod_name == "Mod4") || (mod_name == "Super") || (mod_name == "Logo"))
        {
            mask_bit = (1U << 6);
        }

        if (mask_bit != 0)
        {
            mods_depressed = (mods_depressed & ~mask_bit) | (val ? mask_bit : 0);
            modifiers_changed.emit();
        }
    }
}

bool VirtualKeyboardDevice::is_modifier_pressed(const std::string mod_name)
{
    if ((mod_name == "Lock") || (mod_name == "Caps") || (mod_name == "CapsLock") ||
        (mod_name == "Mod2") || (mod_name == "NumLock") || (mod_name == "ScrollLock"))
    {
        uint32_t mask_bit = 0;
        if ((mod_name == "Lock") || (mod_name == "Caps") || (mod_name == "CapsLock"))
        {
            mask_bit = (1U << 1);
        } else if ((mod_name == "Mod2") || (mod_name == "NumLock"))
        {
            mask_bit = (1U << 4);
        } else if (mod_name == "ScrollLock")
        {
            mask_bit = (1U << 5);
        }

        return (mask_bit != 0) && ((mods_locked & mask_bit) != 0);
    } else
    {
        uint32_t mask_bit = 0;
        if (mod_name == "Shift")
        {
            mask_bit = (1U << 0);
        } else if ((mod_name == "Control") || (mod_name == "Ctrl"))
        {
            mask_bit = (1U << 2);
        } else if ((mod_name == "Mod1") || (mod_name == "Alt"))
        {
            mask_bit = (1U << 3);
        } else if ((mod_name == "Mod4") || (mod_name == "Super") || (mod_name == "Logo"))
        {
            mask_bit = (1U << 6);
        }

        return (mask_bit != 0) && ((mods_depressed & mask_bit) != 0);
    }
}

xkb_keymap*VirtualKeyboardDevice::get_keymap()
{
    return xkb_keymap;
}

void VirtualKeyboardDevice::handle_activate()
{
    WayfireOsk::get().activate();
}

void VirtualKeyboardDevice::handle_deactivate()
{
    WayfireOsk::get().deactivate();
    WayfireOsk::get().get_window().clear_suggestions();
}

void VirtualKeyboardDevice::handle_modifiers(uint32_t mods_depressed, uint32_t mods_latched,
    uint32_t mods_locked, uint32_t group)
{
    modifiers_changed.emit();
}

bool VirtualKeyboardDevice::valid()
{
    return xkb_keymap && xkb_context;
}

uint32_t VirtualKeyboardDevice::get_depressed_modifiers()
{
    return mods_depressed;
}

uint32_t VirtualKeyboardDevice::get_current_layout()
{
    return current_layout;
}

void VirtualKeyboardDevice::accept_suggestion(std::string value)
{
    if (!im)
    {
        return;
    }

    uint32_t bytes_to_delete = im_state.active_word_fragment.length();

    if (bytes_to_delete > 0)
    {
        zwp_input_method_v2_delete_surrounding_text(im, bytes_to_delete, 0);
    }

    std::string text_to_insert = value + " ";

    zwp_input_method_v2_commit_string(im, text_to_insert.c_str());

    zwp_input_method_v2_commit(im, im_state.serial);

    im_state.active_word_fragment = "";
    WayfireOsk::get().get_window().clear_suggestions();
}

bool VirtualKeyboardDevice::is_sensitive()
{
    if (current_hint & HINT_SENSITIVE)
    {
        return true;
    }

    return current_purpose == InputType::PASSWORD || current_purpose == InputType::PIN;
}

bool VirtualKeyboardDevice::is_numeric()
{
    return current_purpose == InputType::DIGITS ||
           current_purpose == InputType::NAME ||
           current_purpose == InputType::PHONE ||
           current_purpose == InputType::PIN ||
           current_purpose == InputType::TIME ||
           current_purpose == InputType::DATETIME;
}
