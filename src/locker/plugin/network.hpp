#pragma once
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <memory>
#include "lockergrid.hpp"
#include "network/network.hpp"
#include "plugin.hpp"
#include "../../util/network/manager.hpp"



class WayfireLockerNetworkPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    WayfireLockerNetworkPluginWidget(std::string image_contents, std::string label_contents,
        std::string css_contents);
    Gtk::Label label;
    Gtk::Image image;
    Gtk::Box box;

    void set_connection(std::shared_ptr<Network>);
};

class WayfireLockerNetworkPlugin : public WayfireLockerPlugin
{
  private:
    std::vector<sigc::connection> signals;

  public:
    WayfireLockerNetworkPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;
    void set_connection(std::shared_ptr<Network> network);
    std::unordered_map<int, std::shared_ptr<WayfireLockerNetworkPluginWidget>> widgets;
    std::shared_ptr<NetworkManager> network_manager;


    std::string image_contents = "network-error-symbolic", label_contents = "Unknown state",
        css_contents = "";
};
