#include "command-output.hpp"

#include <glibmm/main.h>
#include <glibmm/shell.h>
#include <glibmm/spawn.h>

#include <gtkmm/tooltip.h>

#include <gtk-utils.hpp>

static void label_set_from_command(const std::string & command_line,
    Gtk::Label& label)
{
    Glib::Pid pid;
    int output_fd;
    Glib::spawn_async_with_pipes("", Glib::shell_parse_argv(command_line),
        Glib::SPAWN_DO_NOT_REAP_CHILD | Glib::SPAWN_SEARCH_PATH_FROM_ENVP,
        Glib::SlotSpawnChildSetup{}, &pid, nullptr, &output_fd, nullptr);
    Glib::signal_child_watch().connect([=, &label] (Glib::Pid pid, int exit_status)
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
    if (icon_size > 0)
    {
        set_image_icon(icon, icon_name, icon_size, {});
    }

    main_label.set_ellipsize(Pango::ELLIPSIZE_END);
    main_label.set_max_width_chars(max_chars_opt);
    max_chars_opt.set_callback([=]
    {
        main_label.set_max_width_chars(max_chars_opt);
    });
    main_label.set_alignment(Gtk::ALIGN_CENTER);
    max_chars_opt.set_callback([this]
    {
        main_label.set_max_width_chars(max_chars_opt);
    });

    box.set_spacing(5);

    box.set_orientation(
        icon_position == "bottom" ||
        icon_position ==
        "top" ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL);

    if ((icon_position == "right") || (icon_position == "bottom"))
    {
        box.pack_start(main_label);
        box.pack_start(icon);
    } else
    {
        box.pack_start(icon);
        box.pack_start(main_label);
    }

    if (icon_name.empty())
    {
        box.remove(icon);
    }

    box.show_all();
    add(box);
    set_relief(Gtk::RELIEF_NONE);

    const auto update_output = [=] ()
    {
        label_set_from_command(command, main_label);
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

    const auto update_tooltip = [=]
    {
        if (std::time(nullptr) - last_tooltip_update < 1)
        {
            return;
        }

        label_set_from_command(tooltip_command, tooltip_label);
    };

    if (!tooltip_command.empty())
    {
        set_has_tooltip();
        tooltip_label.show();
        signal_query_tooltip().connect([=] (int, int, bool,
                                            const Glib::RefPtr<Gtk::Tooltip>& tooltip)
        {
            update_tooltip();
            tooltip->set_custom(tooltip_label);
            return true;
        });
    }
}

void WfCommandOutputButtons::init(Gtk::HBox *container)
{
    container->pack_start(box, false, false);
    update_buttons();
    commands_list_opt.set_callback([=] { update_buttons(); });
}

void WfCommandOutputButtons::update_buttons()
{
    const auto & opt_value = commands_list_opt.value();
    buttons.clear();
    buttons.reserve(opt_value.size());
    for (const auto & command_info : opt_value)
    {
        buttons.push_back(std::apply([] (auto&&... args)
        {
            return std::make_unique<CommandOutput>(args...);
        }, command_info));
        box.pack_start(*buttons.back(), false, false);
    }

    box.show_all();
}
