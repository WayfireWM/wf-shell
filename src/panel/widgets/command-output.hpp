#pragma once

#include "../widget.hpp"

#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>

#include <vector>
#include <sigc++/connection.h>
#include <wayfire/config/compound-option.hpp>

class WfCommandOutputButtons : public WayfireWidget
{
    struct CommandOutput : public Gtk::Button
    {
        sigc::connection timeout_connection, command_sig;
        std::vector<sigc::connection> signals;

        Gtk::Box box;
        Gtk::Image icon;
        Gtk::Label main_label;

        Gtk::Label tooltip_label;
        time_t last_tooltip_update = 0;

        std::string tooltip_command;

        WfOption<int> max_chars_opt{"panel/commands_output_max_chars"};

        void init();

        CommandOutput(const std::string & name, const std::string & command,
            const std::string & tooltip_command,
            int period, const std::string & icon_name, int icon_size,
            const std::string & icon_position);
        CommandOutput(CommandOutput&&) = delete;
        CommandOutput(const CommandOutput&) = delete;
        CommandOutput& operator =(CommandOutput&&) = delete;
        CommandOutput& operator =(const CommandOutput&) = delete;
        bool query_tooltip(int i, int j, bool k, const std::shared_ptr<Gtk::Tooltip>& tooltip);
        void update_tooltip();
        ~CommandOutput() override;
    };

    Gtk::Box box;
    std::vector<std::unique_ptr<CommandOutput>> buttons;

    WfOption<wf::config::compound_list_t<std::string, std::string, int, std::string,
        int, std::string>> commands_list_opt{"panel/commands"};

  public:
    void init(Gtk::Box *container) override;
    void update_buttons();
};
