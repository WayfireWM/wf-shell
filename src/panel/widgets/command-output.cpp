#include "command-output.hpp"

#include <glibmm/main.h>
#include <glibmm/spawn.h>
#include <iostream>

void WfCommandOutputButtons::CommandOutput::init()
{
    icon.set_from_icon_name(icon_name, Gtk::ICON_SIZE_LARGE_TOOLBAR);
    box.pack_start(icon, true, true);
    output.set_single_line_mode();
    output.set_ellipsize(Pango::ELLIPSIZE_END);
    output.set_max_width_chars(WfOption<int>{"panel/commands_output_max_chars"});
    box.pack_start(output, false, false);
    box.show_all();
    add(box);
    set_relief(Gtk::RELIEF_NONE);

    const auto update_output = [&]() {
        std::string out;
        Glib::spawn_command_line_sync("bash -c \'" + command + "\'", &out);
        output.set_text(out);
    };

    signal_clicked().connect(update_output);

    if (period > 0)
    {
        timeout_connection = Glib::signal_timeout().connect_seconds(
            [=]() {
                update_output();
                return true;
            },
            period);
    }
    if (period != -1)
    {
        update_output();
    }
}

void WfCommandOutputButtons::WfCommandOutputButtons::init(Gtk::HBox *container)
{
    container->pack_start(box, false, false);
    handle_config_reload();
}

void WfCommandOutputButtons::WfCommandOutputButtons::handle_config_reload()
{
    buttons = get_buttons_from_config();
    for (auto &button : buttons)
        box.pack_start(button, false, false);

    box.show_all();
}

inline static bool begins_with(const std::string &string, const std::string &prefix)
{
    return string.size() >= prefix.size() && string.substr(0, prefix.size()) == prefix;
}

std::vector<WfCommandOutputButtons::CommandOutput> WfCommandOutputButtons::WfCommandOutputButtons::
    get_buttons_from_config()
{
    static const std::string PREFIX = "command_output_";
    static const std::string ICON_PREFIX = PREFIX + "icon_";
    static const std::string PERIOD_PREFIX = PREFIX + "period_";

    std::vector<CommandOutput> buttons;
    const auto get_or_insert_button = [&buttons](const std::string &name) -> CommandOutput & {
        const auto button = std::find_if(buttons.begin(), buttons.end(),
                                         [&name](const CommandOutput &button) { return button.name == name; });
        if (button != buttons.end())
        {
            return *button;
        }

        buttons.emplace_back();
        buttons.back().name = name;
        return buttons.back();
    };
    for (const auto &option : WayfireShellApp::get().config.get_section("panel")->get_registered_options())
    {
        if (begins_with(option->get_name(), ICON_PREFIX))
        {
            const std::string name = option->get_name().substr(ICON_PREFIX.size());
            get_or_insert_button(name).icon_name = option->get_value_str();
        }
        else if (begins_with(option->get_name(), PERIOD_PREFIX))
        {
            const std::string name = option->get_name().substr(PERIOD_PREFIX.size());
            const auto period = wf::option_type::from_string<int>(option->get_value_str());
            if (!period)
            {
                std::cerr << "Invalid command period value in config file" << std::endl;
            }
            else
            {
                get_or_insert_button(name).period = period.value();
            }
        }
        else if (begins_with(option->get_name(), PREFIX))
        {
            const std::string name = option->get_name().substr(PREFIX.size());
            get_or_insert_button(name).command = option->get_value_str();
        }
    }
    for (auto &button : buttons)
    {
        button.init();
    }
    return buttons;
}
