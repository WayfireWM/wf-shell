#pragma once

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <memory>
#include <sigc++/connection.h>

#include "wf-popover.hpp"
#include "../widget.hpp"
#include "../../util/network/manager.hpp"
#include "network/network-widget.hpp"

class WayfireNetworkInfo : public WayfireWidget
{
    std::vector<sigc::connection> signals;
    std::shared_ptr<NetworkManager> network_manager;
    std::unique_ptr<WayfireMenuButton> button;
    Gtk::Box button_content;
    Gtk::Image icon;
    Gtk::Label status;
    WfOption<std::string> status_opt{"panel/network_status"};
    WfOption<bool> status_color_opt{"panel/network_status_use_color"};
    WfOption<std::string> status_font_opt{"panel/network_status_font"};
    WfOption<std::string> click_command_opt{"panel/network_onclick_command"};
    
    NetworkControlWidget control;
  Gtk::Window window_undo_me;
  public:
    WayfireNetworkInfo();
    ~WayfireNetworkInfo();
    void init(Gtk::Box *container);
    void on_click();
    void set_connection(std::shared_ptr<Network> network);
};
