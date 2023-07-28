#include "command-output.hpp"

#include <glibmm/main.h>
#include <glibmm/spawn.h>

#include <gtk-utils.hpp>

WfCommandOutputButtons::CommandOutput::CommandOutput(const std::string & name,
    const std::string & command,
    const std::string & icon_name, int period, int icon_size,
    const std::string& icon_position)
{
    if (icon_size > 0)
    {
        set_image_icon(icon, icon_name, icon_size, {});
    }

    output.set_ellipsize(Pango::ELLIPSIZE_END);
    output.set_max_width_chars(max_chars_opt);
    max_chars_opt.set_callback([=] { output.set_max_width_chars(max_chars_opt); });
    output.set_alignment(Gtk::ALIGN_CENTER);
    max_chars_opt.set_callback([this]
    {
        output.set_max_width_chars(max_chars_opt);
    });

    box.set_spacing(5);

    if (icon_position == "left")
    {
        box.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        box.pack_start(icon);
        box.pack_start(output);
    } else if (icon_position == "right")
    {
        box.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        box.pack_start(output);
        box.pack_start(icon);
    } else if (icon_position == "top")
    {
        box.set_orientation(Gtk::ORIENTATION_VERTICAL);
        box.pack_start(icon);
        box.pack_start(output);
    } else
    {
        box.set_orientation(Gtk::ORIENTATION_VERTICAL);
        box.pack_start(output);
        box.pack_start(icon);
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
        Glib::Pid pid;
        int std_out;
        Glib::spawn_async_with_pipes("", std::vector<std::string>{"/bin/bash", "-c",
            command}, Glib::SPAWN_DO_NOT_REAP_CHILD,
            Glib::SlotSpawnChildSetup{},
            &pid, nullptr, &std_out, nullptr);
        Glib::signal_child_watch().connect([std_out, this] (
            Glib::Pid pid,
            int child_status)
        {
            FILE *file = fdopen(std_out, "r");
            char buf[20];
            std::fill(std::begin(buf), std::end(buf), '\0');
            std::fread(buf, sizeof(buf[0]), sizeof(buf) - 1, file);
            Glib::ustring output_str(buf);
            std::fclose(file);
            Glib::spawn_close_pid(pid);

            while (!output_str.empty() && std::isspace(*output_str.rbegin()))
            {
                output_str.erase(std::prev(output_str.end()));
            }

            output.set_text(output_str);
        }, pid);
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
