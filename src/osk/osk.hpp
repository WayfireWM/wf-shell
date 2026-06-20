#pragma once

#include <string>
#include <memory>

#include <gtkmm.h>
#include "complete/complete.hpp"
#include "layout.hpp"
#include "sigc++/connection.h"
#include "virtual-keyboard.hpp"
#include "wayland-window.hpp"
#include "wf-shell-app.hpp"
#include <xkbcommon/xkbcommon.h>

extern int spacing;

class WayfireOsk : public WayfireShellApp
{
    void init_layouts();
    void remove_layout();

    std::vector<sigc::connection> signals;

    std::unique_ptr<WaylandWindow> window;
    std::unique_ptr<VirtualKeyboardDevice> vk = nullptr;
    std::unique_ptr<WayfireOskComplete> complete;
    WayfireOsk();

    Gtk::Box *box = nullptr;
    std::unique_ptr<WayfireOskLayout> layout = nullptr;
    std::string active_layout_path;
    void refresh_labels_from_xkb();

    std::string layout_name = "iso";


    bool start_hidden = false;
    bool activate_show = false, deactivate_hide = false;

    void set_completor();

    std::string user_chosen_layout = "";

  public:
    static WayfireOsk& get();
    static void create(int argc, char **argv);

    VirtualKeyboardDevice& get_device();
    WaylandWindow& get_window();
    WayfireOskComplete& get_complete();
    ~WayfireOsk();

    void activate();
    void deactivate();
    void on_config_reload() override;

    Gio::Application::Flags get_extra_application_flags() override
    {
        return Gio::Application::Flags::NON_UNIQUE;
    }

    std::string get_application_name() override;

    void user_selected_layout(std::string);
    std::string get_current_layout();

  protected:
    void add_output(GMonitor monitor) override
    {}
    void rem_output(GMonitor monitor) override
    {}
    void on_activate() override;
};
