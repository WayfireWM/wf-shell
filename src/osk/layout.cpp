#include "layout.hpp"
#include "glibmm/ustring.h"
#include "gtkmm/togglebutton.h"
#include "osk.hpp"
#include <iostream>

WayfireOskLayout::WayfireOskLayout(std::string xml_filepath)
{
    builder = Gtk::Builder::create_from_file(xml_filepath);
    auto layout = builder->get_widget<Gtk::Box>("keyboard_main");
    if (!layout)
    {
        std::cerr << "Layout box null" << std::endl;
        std::exit(-1);
    }

    append(*layout);
    hook_up();
}

void WayfireOskLayout::hook_up()
{
    /* Iterate all objects */
    GSList *all_objects = gtk_builder_get_objects(builder->gobj());

    for (GSList *l = all_objects; l != nullptr; l = l->next)
    {
        GObject *gobj = G_OBJECT(l->data);

        auto gobject = Glib::wrap(gobj, true);
        if (!gobject)
        {
            continue;
        }

        auto toggle_obj = dynamic_cast<Gtk::ToggleButton*>(gobject.get());
        if (toggle_obj)
        {
            /* Toggles can ONLY be scancode types */
            std::string id = toggle_obj->get_name();
            bind_toggle(toggle_obj, id);
        } else
        {
            /* Buttons might be scancode based or string literal */
            auto button_obj = dynamic_cast<Gtk::Button*>(gobject.get());
            if (button_obj)
            {
                std::string id = button_obj->get_name();
                if (id.rfind("gtkmm", 0) == 0)
                {
                    /* Button without a scan-code name. Sends its label as content */
                    auto click_gesture = Gtk::GestureClick::create();

                    signals.push_back(click_gesture->signal_pressed().connect(
                        [=] (int button, double x, double y)
                    {
                        click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
                    }));
                    signals.push_back(click_gesture->signal_released().connect(
                        [=] (int button, double x, double y)
                    {
                        auto& keyboard = WayfireOsk::get();
                        keyboard.get_device().send_string(button_obj->get_label());
                    }));
                    button_obj->add_controller(click_gesture);
                } else
                {
                    /* Scancode mode*/
                    bind(button_obj, id);
                }
            }

            auto label_obj = dynamic_cast<Gtk::Label*>(gobject.get());
            if (label_obj)
            {
                std::string id = label_obj->get_name();
                if (id.rfind("gtkmm", 0) == 0)
                {
                    auto click_gesture = Gtk::GestureClick::create();

                    signals.push_back(click_gesture->signal_pressed().connect(
                        [=] (int button, double x, double y)
                    {
                        click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);
                    }));
                    signals.push_back(click_gesture->signal_released().connect(
                        [=] (int button, double x, double y)
                    {
                        auto& keyboard = WayfireOsk::get();
                        keyboard.get_device().send_string(label_obj->get_label());
                    }));
                    label_obj->add_controller(click_gesture);
                } else
                {
                    /* For sanity sake we ARE NOT allowing labels as scancodes */
                    std::cerr << "error label with possible scancode '" << id << "'." << std::endl;
                }
            }
        }
    }

    g_slist_free(all_objects);
}

WayfireOskLayout::~WayfireOskLayout()
{
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

void WayfireOskLayout::bind(Gtk::Button *button, std::string id)
{
    auto keymap = WayfireOsk::get().get_device().get_keymap();
    xkb_keycode_t kc = xkb_keymap_key_by_name(keymap, id.c_str());

    auto update_label = [=] ()
    {
        auto keymap = WayfireOsk::get().get_device().get_keymap();
        if ((kc == XKB_KEYCODE_INVALID) || (kc < 8))
        {
            return;
        }

        auto layout_idx = WayfireOsk::get().get_device().get_current_layout();

        auto symbol = get_keycap_symbol(id);

        if (id.compare("SPCE") == 0)
        {
            /* TODO Option*/
            const char *layout_name = xkb_keymap_layout_get_name(
                keymap, layout_idx);

            button->set_label(layout_name);
            return;
        }

        if (!symbol.empty())
        {
            button->set_label(symbol);
            return;
        }

        struct xkb_state *temp_state = xkb_state_new(keymap);
        if (!temp_state)
        {
            std::cout << "error, no temp_state" << std::endl;
            return;
        }

        auto depressed = WayfireOsk::get().get_device().get_depressed_modifiers();

        xkb_state_update_mask(
            temp_state,
            depressed,
            0,
            0,
            0,
            0,
            0);
        xkb_layout_index_t num_layouts = xkb_keymap_num_layouts(keymap);
        if (layout_idx >= num_layouts)
        {
            std::cerr << "Invalid layout idx " << layout_idx << " : Max " << num_layouts << std::endl;
            return;
        }

        xkb_level_index_t level = xkb_state_key_get_level(temp_state, kc, layout_idx);
        if (level == XKB_LEVEL_INVALID)
        {
            return;
        }

        const xkb_keysym_t *syms;
        int num_syms = xkb_keymap_key_get_syms_by_level(keymap, kc,
            layout_idx, level, &syms);
        if (num_syms > 0)
        {
            char buffer[7];

            int bytes_written = xkb_keysym_to_utf8(*syms, buffer, sizeof(buffer));

            if (bytes_written > 0)
            {
                std::string label = std::string(buffer, bytes_written);
                if (!label.empty())
                {
                    button->set_label(label);
                }
            }
        }
    };
    /* Set a label and update every state change */
    update_label();
    signals.push_back(WayfireOsk::get().get_device().signal_modifiers_changed().connect([=] ()
    {
        update_label();
    }));
    signals.push_back(WayfireOsk::get().get_device().signal_layer_changed().connect([=] ()
    {
        update_label();
    }));

    /* A non-modifier key */
    auto click_gesture = Gtk::GestureClick::create();

    signals.push_back(click_gesture->signal_pressed().connect(
        [=] (int button, double x, double y)
    {
        auto& keyboard = WayfireOsk::get();
        auto code = kc - 8;

        click_gesture->set_state(Gtk::EventSequenceState::CLAIMED);

        keyboard.get_device().send_key(code,
            WL_KEYBOARD_KEY_STATE_PRESSED);
    }));
    signals.push_back(click_gesture->signal_released().connect(
        [=] (int button, double x, double y)
    {
        auto& keyboard = WayfireOsk::get();
        auto code = kc - 8;

        keyboard.get_device().send_key(code,
            WL_KEYBOARD_KEY_STATE_RELEASED);
    }));

    button->add_controller(click_gesture);
}

void WayfireOskLayout::bind_toggle(Gtk::ToggleButton *button, std::string name)
{
    /* Code path for modifier keys. For now we latch them Only. Awaiting bug reports from people who
     * expect to send a CTRl alone etc */
    std::string modifier_name = "";
    if ((name.compare("RCTL") == 0) || (name.compare("LCTL") == 0))
    {
        modifier_name = XKB_MOD_NAME_CTRL;
    } else if ((name.compare("LFSH") == 0) || (name.compare("RFSH") == 0))
    {
        modifier_name = XKB_MOD_NAME_SHIFT;
    } else if ((name.compare("LALT") == 0) || (name.compare("RALT") == 0))
    {
        modifier_name = XKB_MOD_NAME_ALT;
    } else if ((name.compare("LWIN") == 0) || (name.compare("RWIN") == 0))
    {
        modifier_name = XKB_MOD_NAME_LOGO;
    } else if (name.compare("CAPS") == 0)
    {
        modifier_name = XKB_MOD_NAME_CAPS;
    } else if ((name.compare("NMLK") == 0) || ((name.compare("KPNM")) == 0))
    {
        modifier_name = XKB_MOD_NAME_NUM;
    }

    if (modifier_name.empty())
    {
        std::cout << "Not Modifier " << modifier_name << "  " << name << std::endl;
        return;
    }

    auto update_label = [=] ()
    {
        auto symbol = get_keycap_symbol(name);
        button->set_label(symbol);
    };
    /* Set a label and update every state change */
    update_label();
    size_t toggle_index = signals.size();
    auto toggle_sig     = button->signal_toggled().connect([=] ()
    {
        WayfireOsk::get().get_device().toggle_modifier(modifier_name);
    });

    signals.push_back(toggle_sig);

    signals.push_back(WayfireOsk::get().get_device().signal_modifiers_changed().connect([=] ()
    {
        /* This realistically is disconnected directly after the above, so this hack is fine */
        signals[toggle_index].block();
        button->set_active(WayfireOsk::get().get_device().is_modifier_pressed(modifier_name));
        signals[toggle_index].unblock();
    }));
}

std::string WayfireOskLayout::get_keycap_symbol(std::string id)
{
    if (id.compare("LEFT") == 0)
    {
        return "←";
    } else if (id.compare("RGHT") == 0)
    {
        return "→";
    } else if (id.compare("UP") == 0)
    {
        return "↑";
    } else if (id.compare("DOWN") == 0)
    {
        return "↓";
    } else if (id.compare("BKSP") == 0)
    {
        return "⌫";
    } else if (id.compare("DEL") == 0)
    {
        return "⌦";
    } else if ((id.compare("RTRN") == 0) || (id.compare("KPEN") == 0))
    {
        return "↵";
    } else if (id.compare("TAB") == 0)
    {
        return "⇥";
    } else if ((id.compare("LFSH") == 0) || (id.compare("RTSH") == 0))
    {
        return "⇧";
    } else if ((id.compare("LCTL") == 0) || (id.compare("RCTL") == 0))
    {
        return "⌃";
    } else if ((id.compare("LALT") == 0) || (id.compare("RALT") == 0))
    {
        return "⌥";
    } else if (id.compare("CAPS") == 0)
    {
        return "⇪";
    } else if ((id.compare("LWIN") == 0) || (id.compare("RWIN") == 0))
    {
        return "⌘";
    } else if (id.compare("ESC") == 0)
    {
        return "ESC";
    } else if (id.compare("SPCE") == 0)
    {
        return "␣";
    } else if (id.compare("KPNM") == 0)
    {
        return "NUM";
    }

    return "";
}
