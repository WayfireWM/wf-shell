#include <gtkmm/cssprovider.h>
#include <glibmm/main.h>
#include <glibmm/shell.h>
#include <glibmm/spawn.h>

#include <gtkmm/tooltip.h>

#include <array>
#include <ctime>

#include <gtk-utils.hpp>
#include <string>

#include "command-output.hpp"

static sigc::connection label_set_from_command(std::string command_line,
    Gtk::Label& label)
{
    command_line = "/bin/sh -c \"" + command_line + "\"";

    Glib::Pid pid;
    int output_fd;
    Glib::spawn_async_with_pipes("", Glib::shell_parse_argv(command_line),
        Glib::SpawnFlags::DO_NOT_REAP_CHILD | Glib::SpawnFlags::SEARCH_PATH_FROM_ENVP,
        Glib::SlotSpawnChildSetup{}, &pid, nullptr, &output_fd, nullptr);
    return Glib::signal_child_watch().connect([=, &label] (Glib::Pid pid, int exit_status)
    {
        FILE *file = fdopen(output_fd, "r");
        Glib::ustring output;
        std::array<char, 16> buffer;

        while (std::fgets(buffer.data(), buffer.size(), file))
        {
            output += buffer.data();
        }

        std::fclose(file);
        Glib::spawn_close_pid(pid);

        while (!output.empty() && std::isspace(*output.rbegin()))
        {
            output.erase(std::prev(output.end()));
        }

        label.set_markup(output);
    }, pid);
}

WfCommandOutputButtons::CommandOutput::CommandOutput(const std::string & name,
    const std::string & command, const std::string & tooltip_command, int period,
    const std::string & icon_name, int icon_size,
    const std::string & icon_position)
{
    this->tooltip_command = tooltip_command;

    image_set_icon(&icon, icon_name);

    if (icon_size > 0)
    {
        css_provider = Gtk::CssProvider::create();
        css_provider->load_from_string(".command-icon-" + name + "{-gtk-icon-size:" + std::to_string(
            icon_size) + "px;}");
        icon.add_css_class("command-icon-" + name);
        icon.get_style_context()->add_provider(css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
    }

    icon.add_css_class("widget-icon");
    add_css_class("command-output");
    add_css_class("icon-" + icon_position);

    main_label.set_ellipsize(Pango::EllipsizeMode::END);
    main_label.set_max_width_chars(max_chars_opt);
    max_chars_opt.set_callback([=]
    {
        main_label.set_max_width_chars(max_chars_opt);
    });
    // main_label.set_alignment(Gtk::ALIGN_CENTER);
    max_chars_opt.set_callback([this]
    {
        main_label.set_max_width_chars(max_chars_opt);
    });

    box.set_orientation(
        icon_position == "bottom" ||
        icon_position ==
        "top" ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL);

    if ((icon_position == "right") || (icon_position == "bottom"))
    {
        box.append(main_label);
        box.append(icon);
    } else
    {
        box.append(icon);
        box.append(main_label);
    }

    if (icon_name.empty())
    {
        box.remove(icon);
    }

    set_child(box);
    // set_relief(Gtk::RELIEF_NONE);

    const auto update_output = [=] ()
    {
        command_sig.disconnect();
        command_sig = label_set_from_command(command, main_label);
    };

    signals.push_back(signal_clicked().connect(update_output));

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

    if (!tooltip_command.empty())
    {
        set_has_tooltip();
        tooltip_label.show();
        signals.push_back(signal_query_tooltip().connect(sigc::mem_fun(*this,
            &WfCommandOutputButtons::CommandOutput::query_tooltip), false));
    }
}

WfCommandOutputButtons::CommandOutput::~CommandOutput()
{
    timeout_connection.disconnect();
    for (auto signal : signals)
    {
        signal.disconnect();
    }
}

bool WfCommandOutputButtons::CommandOutput::query_tooltip(int i, int j, bool k,
    const std::shared_ptr<Gtk::Tooltip>& tooltip)
{
    this->update_tooltip();
    tooltip->set_custom(tooltip_label);
    return true;
}

void WfCommandOutputButtons::CommandOutput::update_tooltip()
{
    if (std::time(nullptr) - last_tooltip_update < 1)
    {
        return;
    }

    label_set_from_command(tooltip_command, tooltip_label);
}

void WfCommandOutputButtons::init(Gtk::Box *container)
{
    box.add_css_class("command-output-box");
    container->append(box);
    update_buttons();
    commands_list_opt.set_callback([=] { update_buttons(); });
}

void WfCommandOutputButtons::update_buttons()
{
    const auto & opt_value = commands_list_opt.value();
    for (auto child : box.get_children())
    {
        box.remove(*child);
    }

    buttons.clear();
    buttons.reserve(opt_value.size());
    for (const auto & command_info : opt_value)
    {
        buttons.push_back(std::apply([] (auto&&... args)
        {
            return std::make_unique<CommandOutput>(args...);
        }, command_info));
        box.append(*buttons.back());
    }
}
