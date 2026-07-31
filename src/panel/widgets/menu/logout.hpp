#pragma once
#include <string>
#include <gtkmm/centerbox.h>
#include <gtkmm/grid.h>

#include <wf-option-wrap.hpp>

#include "logoutbutton.hpp"

class WayfireLogoutUI
{
  public:
    WayfireLogoutUI();
    ~WayfireLogoutUI();
    WfOption<std::string> logout_command{"panel/logout_command"};
    WfOption<std::string> reboot_command{"panel/reboot_command"};
    WfOption<std::string> shutdown_command{"panel/shutdown_command"};
    WfOption<std::string> suspend_command{"panel/suspend_command"};
    WfOption<std::string> hibernate_command{"panel/hibernate_command"};
    WfOption<std::string> switchuser_command{"panel/switchuser_command"};
    Gtk::Window ui;
    WayfireLogoutUIButton logout;
    WayfireLogoutUIButton reboot;
    WayfireLogoutUIButton shutdown;
    WayfireLogoutUIButton suspend;
    WayfireLogoutUIButton hibernate;
    WayfireLogoutUIButton switchuser;
    WayfireLogoutUIButton cancel;
    Gtk::CenterBox box;
    Gtk::Grid main_layout;
    std::vector<sigc::connection> signals;
    void create_logout_ui_button(WayfireLogoutUIButton *button,
        const char *icon, const char *label);
    void on_logout_click();
    void on_reboot_click();
    void on_shutdown_click();
    void on_suspend_click();
    void on_hibernate_click();
    void on_switchuser_click();
    void on_cancel_click();
};
