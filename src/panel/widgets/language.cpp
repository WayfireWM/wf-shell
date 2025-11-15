#include <cstddef>
#include <cstdint>
#include <glibmm.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <wayfire/util/log.hpp>
#include <xkbcommon/xkbregistry.h>
#include "language.hpp"
#include "gtkmm/button.h"
#include "sigc++/functors/mem_fun.h"

void WayfireLanguage::init(Gtk::Box *container)
{
    button.get_style_context()->add_class("language");
    button.get_style_context()->add_class("flat");
    button.get_style_context()->remove_class("activated");
    button.signal_clicked().connect(sigc::mem_fun(*this, &WayfireLanguage::next_layout));
    button.show();

    ipc->subscribe(this, {"keyboard-modifier-state-changed"});
    ipc->send("{\"method\":\"wayfire/get-keyboard-state\"}", [=] (wf::json_t data)
    {
        set_available(data["possible-layouts"]);
        set_current(data["layout-index"]);
    });

    container->append(button);
}

void WayfireLanguage::on_event(wf::json_t data)
{
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

bool WayfireLanguage::update_label()
{
    if (current_layout >= available_layouts.size())
    {
        return false;
    }

    button.set_label(available_layouts[current_layout].ID);
    return true;
}

void WayfireLanguage::set_current(uint32_t index)
{
    current_layout = index;
    update_label();
}

void WayfireLanguage::set_available(wf::json_t layouts)
{
    std::vector<Layout> layouts_available;
    std::map<std::string, uint32_t> names;

    for (size_t i = 0; i < layouts.size(); i++)
    {
        auto elem = layouts[i];
        names[elem] = i;
        layouts_available.push_back(Layout{
            .Name = (std::string)elem,
            .ID   = "",
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
        }
    }

    available_layouts = layouts_available;
    update_label();
}

void WayfireLanguage::next_layout()
{
    uint32_t next = current_layout + 1;
    if (next >= available_layouts.size())
    {
        next = 0;
    }

    wf::json_t message;
    message["method"] = "wayfire/set-keyboard-state";
    message["data"]   = wf::json_t();
    message["data"]["layout-index"] = next;
    ipc->send(message.serialize());
}

WayfireLanguage::WayfireLanguage(std::shared_ptr<WayfireIPC> ipc) : ipc(ipc)
{}

WayfireLanguage::~WayfireLanguage()
{
    ipc->unsubscribe(this);
}
