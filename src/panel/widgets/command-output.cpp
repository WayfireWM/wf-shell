#include "command-output.hpp"

#include <glibmm/main.h>
#include <glibmm/spawn.h>
#include <iostream>

WfCommandOutputButtons::CommandOutput::CommandOutput(const std::string & name,
    const std::string & command,
    const std::string & icon_name, int period) :
    name(name), command(command), icon_name(icon_name), period(period)
{
    icon.set_from_icon_name(icon_name, Gtk::ICON_SIZE_LARGE_TOOLBAR);
    box.pack_start(icon, true, true);
    output.set_single_line_mode();
    output.set_ellipsize(Pango::ELLIPSIZE_END);
    output.set_max_width_chars(max_chars_opt);
    max_chars_opt.set_callback([this]
    {
        output.set_max_width_chars(max_chars_opt);
    });
    box.pack_start(output, false, false);
    box.show_all();
    add(box);
    set_relief(Gtk::RELIEF_NONE);

    const auto update_output = [this] ()
    {
        std::string out;
        // TODO: use spawn_async
        Glib::spawn_command_line_sync("bash -c \'" + this->command + "\'", &out);
        output.set_text(out);
    };

    signal_clicked().connect(update_output);

    if (period > 0)
    {
        timeout_connection = Glib::signal_timeout().connect_seconds([=] ()
        {
            update_output();
            return true;
        }, period);
    }

    if (period != -1)
    {
        update_output();
    }
}

void WfCommandOutputButtons::init(Gtk::HBox *container)
{
    container->pack_start(box, false, false);
    update_buttons();
    commands_list_opt.set_callback([=] { handle_config_reload(); });
}

void WfCommandOutputButtons::update_buttons()
{
    const auto & opt_value = commands_list_opt.value();
    buttons.clear();
    buttons.reserve(opt_value.size());
    for (const auto & [name, command, icon_name, period] : opt_value)
    {
        buttons.push_back(std::make_unique<CommandOutput>(name, command, icon_name,
            period));
        box.pack_start(*buttons.back(), false, false);
    }

    box.show_all();
}

inline static bool begins_with(const std::string & string,
    const std::string & prefix)
{
    return string.size() >= prefix.size() &&
           string.substr(0, prefix.size()) == prefix;
}
