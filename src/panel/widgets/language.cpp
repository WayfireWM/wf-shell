#include <iostream>
#include <cstddef>
#include <cstdint>

#include <glibmm.h>
#include <gtkmm/button.h>

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <wayfire/util/log.hpp>
#include <xkbcommon/xkbregistry.h>
#include "../widget.hpp"
#include "wf-popover.hpp"

#include "language.hpp"
#include "wf-ipc.hpp"
#include "panel.hpp"

void WayfireLanguage::init(Gtk::HBox *container)
{

    ipc_client  = WayfirePanelApp::get().get_ipc_server_instance()->create_client();

    if (!ipc_client)
    {
        std::cout << "Failed to connect to ipc. (are ipc and ipc-rules plugins loaded?)";

    }

    auto style = button.get_style_context();
    style->add_class("flat");
    style->add_class("language");
    style->remove_class("activated");
    btn_sig = button.signal_clicked().connect(sigc::mem_fun(*this, &WayfireLanguage::next_layout));

    container->pack_start(button, Gtk::PACK_SHRINK);
  
    button.show_all();  
    ipc_client->subscribe(this, {"keyboard-modifier-state-changed"});
    ipc_client->send("{\"method\":\"wayfire/get-keyboard-state\"}", [=] (wf::json_t data)
    {
        if (data.serialize().find(
            "error") != std::string::npos)
        {
            std::cerr << "Error getting keyboard state for language widget. Is wayfire ipc-rules plugin enabled?" << std::endl;
            return;
        }

        set_available(data["possible-layouts"]);
        set_current(data["layout-index"]);
    });
}

void WayfireLanguage::on_event(wf::json_t data)
{
	std::cout <<"on event/n";
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
	std::cout << "update label /n";
    if (current_layout >= available_layouts.size())
    {
        return false;
    }

    button.set_label(available_layouts[current_layout].ID);
    return true;
}

void WayfireLanguage::set_current(uint32_t index)
{
	std::cout << "set_current /n";
    current_layout = index;
    update_label();
}

void WayfireLanguage::set_available(wf::json_t layouts)
{
	std::cout << "set_available/n" ;
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
	std::cout << "next layout /n";
    uint32_t next = current_layout + 1;
    if (next >= available_layouts.size())
    {
        next = 0;
    }

    wf::json_t message;
    message["method"] = "wayfire/set-keyboard-state";
    message["data"]   = wf::json_t();
    message["data"]["layout-index"] = next;
    ipc_client->send(message.serialize());
}

WayfireLanguage::WayfireLanguage()
{}

WayfireLanguage::~WayfireLanguage()
{
    if (ipc_client)
    {
        ipc_client->unsubscribe(this);
    }

    btn_sig.disconnect();
}
