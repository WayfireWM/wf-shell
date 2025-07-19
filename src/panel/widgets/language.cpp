#include <cstdint>
#include <glibmm.h>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <xkbcommon/xkbregistry.h>
#include "language.hpp"
#include "gtkmm/button.h"
#include "libutil.a.p/wayfire-shell-unstable-v2-client-protocol.h"
#include "sigc++/functors/mem_fun.h"

void WayfireLanguage::init(Gtk::HBox *container)
{
    // button = std::make_unique<Gtk::Button>("panel");

    button.get_style_context()->add_class("language");
    button.add(label);
    button.get_style_context()->add_class("flat");
    button.get_style_context()->remove_class("activated");
    button.signal_clicked().connect_notify(sigc::mem_fun(this, &WayfireLanguage::next_language));
    button.show();
    label.show();

    update_label();

    container->pack_start(button, false, false);
}

bool WayfireLanguage::update_label()
{
    if (current_language >= available.size()) {
        return false;
    }

    label.set_text(available[current_language].ID);
    return true;
}

static void keyboard_lang_manager_current_layout(void *data, struct zwf_keyboard_lang_manager_v2 *keyboard_lang_manager, const uint32_t id)
{
    auto wf_language = (WayfireLanguage*)data;
    wf_language->set_current(id);
    wf_language->update_label();
}

static void keyboard_lang_manager_available_layouts(void *data, struct zwf_keyboard_lang_manager_v2 *keyboard_lang_manager, wl_array *layouts)
{
    std::vector<Language> languages;
    std::map<std::string, uint32_t> names;
    char *elem = (char *) layouts->data;
    uint32_t index = 0;
    while (elem < (char *) layouts->data + layouts->size) {
        names[std::string(elem)] = index;
        languages.push_back(Language{
            .Name = elem,
            .ID = "",
        });
        size_t length = strlen(elem) + 1;
        elem += length;
        index++;
    }

    auto context = rxkb_context_new(RXKB_CONTEXT_NO_FLAGS);
    rxkb_context_parse_default_ruleset(context);
    auto rlayout = rxkb_layout_first(context);
    for (; rlayout != NULL; rlayout = rxkb_layout_next(rlayout)) {
        auto descr = rxkb_layout_get_description(rlayout);
        auto name = names.find(descr);
        if (name != names.end()) {
            languages[name->second].ID = rxkb_layout_get_brief(rlayout);
        }
    }

    auto wf_language = (WayfireLanguage*)data;
    wf_language->set_available(languages);
}

static zwf_keyboard_lang_manager_v2_listener listener = {
    .current_layout = keyboard_lang_manager_current_layout,
    .available_layouts = keyboard_lang_manager_available_layouts,
};


void WayfireLanguage::set_current(uint32_t index)
{
    current_language = index;
}

void WayfireLanguage::set_available(std::vector<Language> languages)
{
    available = languages;
}

void WayfireLanguage::next_language()
{
    uint32_t next = current_language + 1;
    if (next >= available.size())
    {
        next = 0;
    }

    zwf_keyboard_lang_manager_v2_set_layout(keyboard_lang_manager, next);
}

WayfireLanguage::WayfireLanguage(zwf_keyboard_lang_manager_v2 *keyboard_lang_manager): keyboard_lang_manager(keyboard_lang_manager)
{
    zwf_keyboard_lang_manager_v2_add_listener(keyboard_lang_manager, &listener, this);
}

WayfireLanguage::~WayfireLanguage()
{}
