#ifndef COMMAND_OUTPUT_HPP
#define COMMAND_OUTPUT_HPP

#include "../widget.hpp"
#include "wf-popover.hpp"

#include <gtkmm/image.h>
#include <gtkmm/scrolledwindow.h>

class WfCommandOutputButtons : public WayfireWidget
{
    struct CommandOutput : public Gtk::Button
    {
        std::string name;
        std::string command;
        std::string icon_name = "text-x-script";
        int period = 0;

        sigc::connection timeout_connection;

        Gtk::VBox box;
        Gtk::Image icon;
        Gtk::Label output;

        void init();

        CommandOutput() = default;
        CommandOutput(CommandOutput &&) = default;
        ~CommandOutput() override
        {
            timeout_connection.disconnect();
        }
    };

    Gtk::HBox box;
    std::vector<CommandOutput> buttons;
    static std::vector<CommandOutput> get_buttons_from_config();

    public:
    void init(Gtk::HBox *container) override;
    void handle_config_reload() override;
};

#endif /* end of include guard: COMMAND_OUTPUT_HPP */
