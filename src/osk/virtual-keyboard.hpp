#pragma once

#include "input-method-unstable-v2-client-protocol.h"
#include "sigc++/signal.h"
#include "wf-ipc.hpp"
#include <cstdint>
#include <string>
#include <virtual-keyboard-unstable-v1-client-protocol.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>

#define HINT_COMPLETION 0x1
#define HINT_SPELL_CHECK    0x2
#define HINT_AUTO_CAPS  0x4
#define HINT_LOWERCASE  0x8
#define HINT_UPPERCASE  0x10
#define HINT_TITLECASE  0x20
#define HINT_HIDDEN_TEXT    0x40
#define HINT_SENSITIVE  0x80
#define HINT_LATIN  0x100
#define HINT_MULTILINE  0x200

/* https://wayland.app/protocols/text-input-unstable-v3#zwp_text_input_v3:enum:content_hint */
enum InputType
{
    ANY      = 0,
    ALPHA    = 1, /* Only allow alphabetic */
    DIGITS   = 1, /* Only digits */
    NUMBER   = 3, /* Digits, period and minus */
    PHONE    = 4,
    URL      = 5,
    EMAIL    = 6,
    NAME     = 7,
    PASSWORD = 8,
    PIN      = 9,
    TIME     = 10,
    DATETIME = 11,
    TERMINAL = 12,
};

struct Layout
{
    std::string Name;
    std::string ID;
    std::string Locale;
};

class VirtualKeyboardDevice : public IIPCSubscriber
{
    sigc::signal<void(void)> keymap_changed, layer_changed, modifiers_changed, hints_changed;
    sigc::signal<void(bool)> ready_changed;
    int shift_pressed_counter = 0;

    zwp_virtual_keyboard_v1 *vk = nullptr;
    zwp_input_method_v2 *im     = nullptr;


    std::shared_ptr<IPCClient> ipc_client;
    std::shared_ptr<WayfireIPC> ipc_server;

    uint32_t mods_depressed = 0, mods_locked = 0;
    bool is_active = false;

    struct xkb_context *xkb_context = nullptr;
    struct xkb_keymap *xkb_keymap   = nullptr;

    std::string keymap;

    void set_current(uint32_t index);
    void set_available(wf::json_t layouts);
    int create_shm_file(size_t size);
    void send_keymap();

  public:
    uint32_t current_layout = 0;
    std::vector<Layout> available_layouts;
    uint32_t current_hint    = 0;
    uint32_t current_purpose = 0;

    VirtualKeyboardDevice();
    ~VirtualKeyboardDevice();

    void on_event(wf::json_t data) override;

    void send_key(uint32_t key, uint32_t state) const;
    void send_key_raw(uint32_t key, uint32_t state) const;

    void send_string(std::string message);
    struct xkb_keymap *get_keymap();
    void set_keymap(std::string map);
    void toggle_modifier(std::string mod_name);
    void set_modifier(std::string mod_name, bool val);
    uint32_t hex_char_to_keycode(char c);
    std::vector<uint32_t> utf8_to_codepoints(const std::string& str);


    bool is_modifier_pressed(std::string mod_name);
    bool valid();

    void handle_keymap(void *data, uint32_t format,
        int32_t fd, uint32_t size);

    void handle_modifiers(uint32_t mods_depressed, uint32_t mods_latched,
        uint32_t mods_locked, uint32_t group);

    void handle_activate();
    void handle_deactivate();

    void accept_suggestion(std::string sugg);

    uint32_t get_depressed_modifiers();
    uint32_t get_current_layout();

    sigc::signal<void(void)> signal_keymap_changed()
    {
        return keymap_changed;
    }

    sigc::signal<void(void)> signal_modifiers_changed()
    {
        return modifiers_changed;
    }

    sigc::signal<void(void)> signal_layer_changed()
    {
        return layer_changed;
    }

    sigc::signal<void(bool)> signal_ready_changed()
    {
        return ready_changed;
    }

    sigc::signal<void(void)> signal_hints_changed()
    {
        return hints_changed;
    }

    bool is_sensitive();
    bool is_numeric();
    void done();
};
