#include "complete/complete-enchant.hpp"
#ifdef HAVE_LLAMA
    #include "complete/complete-tiny.hpp"
#endif
#include "complete/complete.hpp"
#include "css-config.hpp"
#include "gtk/gtk.h"
#include "gtkmm.h"
#include "layout.hpp"
#include "wayland-window.hpp"
#include "wf-option-wrap.hpp"
#include "wf-shell-app.hpp"
#include "osk.hpp"
#include <cstddef>
#include <cstdio>
#include <getopt.h>
#include <iostream>
#include <linux/input-event-codes.h>

#include <memory>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>


int spacing = OSK_SPACING;
int default_width  = 800;
int default_height = 400;
int headerbar_size = 60;

std::string anchor = "bottom";

void WayfireOsk::remove_layout()
{
    if (layout)
    {
        box->remove(*layout);
        layout = nullptr;
        for (auto signal : signals)
        {
            signal.disconnect();
        }

        signals.clear();
    }
}

void WayfireOsk::init_layouts()
{
    std::cout << "init Layout" << std::endl;
    std::string xml_filepath = std::string(LAYOUT_DIR) + "/" + get_current_layout() + ".xml";
    remove_layout();

    if ((vk == nullptr) || !vk->valid())
    {
        std::cout << "Invalid virtual keyboard state. No layout" << std::endl;
        return;
    }

    try {
        std::cout << "Loading layout '" << xml_filepath << "'" << std::endl;

        layout = std::make_unique<WayfireOskLayout>(xml_filepath);
        box->append(*layout);
        active_layout_path = xml_filepath;
    } catch (const Glib::Error& ex)
    {
        std::cerr << "XML Parse Exception Encountered: " << ex.what() << std::endl;
    }

    window->set_widget(*box);
}

WayfireOsk::WayfireOsk()
{}

WayfireOsk::~WayfireOsk()
{}

void WayfireOsk::create(int argc, char **argv)
{
    if (instance)
    {
        throw std::logic_error("Creating keyboard twice!");
    }

    instance = std::unique_ptr<WayfireShellApp>(new WayfireOsk{});
    instance->init_app();
    instance->run(argc, argv);
}

WayfireOsk& WayfireOsk::get()
{
    if (!instance)
    {
        throw std::logic_error("Getting keyboard before creating it!");
    }

    return dynamic_cast<WayfireOsk&>(*instance.get());
}

VirtualKeyboardDevice& WayfireOsk::get_device()
{
    return *vk;
}

WaylandWindow& WayfireOsk::get_window()
{
    return *window;
}

#ifdef HAVE_ENCHANT
WayfireOskComplete& WayfireOsk::get_complete()
{
    return *complete;
}

#endif

void WayfireOsk::activate()
{
    if (activate_show)
    {
        window->show();
    }
}

void WayfireOsk::deactivate()
{
    if (deactivate_hide)
    {
        window->hide();
    }
}

std::string WayfireOsk::get_application_name()
{
    return "org.wayfire.osk";
}

void WayfireOsk::set_completor()
{
    std::string complete_type = WfOption<std::string>{"osk/suggest_engine"};

    if (complete_type.compare("enchant") == 0)
    {
#ifdef HAVE_ENCHANT
        complete = std::make_unique<WayfireOskCompleteEnchant>();
        return;
#else

        std::cerr << "Enchant chosen but compiled out..." << std::endl;
#endif
    } else if (complete_type.compare("llama") == 0)
    {
#ifdef HAVE_LLAMA
        complete = std::make_unique<WayfireOskCompleteTinyLlama>();
        return;
#else
        std::cerr << "Llama chosen but compiled out..." << std::endl;
#endif
    }

    std::cout << "No Auto complete" << std::endl;
    complete = std::make_unique<WayfireOskCompleteNull>();
}

void WayfireOsk::on_activate()
{
    WayfireShellApp::on_activate();
    box    = new Gtk::Box();
    window = std::make_unique<WaylandWindow>(default_width, default_height, anchor, headerbar_size);
    vk     = std::make_unique<VirtualKeyboardDevice>();

    set_completor();
    signals.push_back(vk->signal_ready_changed().connect([=] (bool ready)
    {
        if (ready)
        {
            init_layouts();
        } else
        {
            remove_layout();
        }
    }));
    signals.push_back(vk->signal_keymap_changed().connect([=] ()
    {
        init_layouts();
    }));
    app->add_window(WayfireOsk::get().get_window());
    app->hold();
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data(
        "button.depressed, button.depressed:hover { \
                    background-color: alpha(currentColor, 0.12); \
                    box-shadow: inset 0 2px 5px rgba(0, 0, 0, 0.25); \
                    background-image: none; \
                  }");
    Gtk::StyleContext::add_provider_for_display(WayfireOsk::get().get_window().get_display(),
        css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    new CssFromConfigInt("osk/emoji_size", "label.emoji{font-size:", "px;}");

    if (start_hidden)
    {
        WayfireOsk::get().get_window().hide();
    }
}

void WayfireOsk::on_config_reload()
{
    std::string new_layout_name = WfOption<std::string>("osk/shape");
    if (new_layout_name.compare(layout_name) != 0)
    {
        layout_name = new_layout_name;
        init_layouts();
    }

    activate_show   = WfOption<bool>("osk/activate_show");
    deactivate_hide = WfOption<bool>("osk/deactivate_hide");
    set_completor();
}

std::string WayfireOsk::get_current_layout()
{
    if (!user_chosen_layout.empty())
    {
        return user_chosen_layout;
    }

    if (vk && vk->is_numeric())
    {
        return "numpad";
    }

    return WfOption<std::string>("osk/shape");
}

void WayfireOsk::user_selected_layout(std::string layout)
{
    user_chosen_layout = layout;
    init_layouts();
}

int main(int argc, char **argv)
{
    WayfireOsk::create(argc, argv);
    return 0;
}
