#pragma once
#include <unordered_map>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/grid.h>
#include <glibmm/refptr.h>

#include "../plugin.hpp"
#include "lockergrid.hpp"
#include "timedrevealer.hpp"

class WayfireLockerPinPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    WayfireLockerPinPluginWidget();
    ~WayfireLockerPinPluginWidget();
    Gtk::Grid grid;
    Gtk::Button bsub, bcan;
    Gtk::Label label;
    void init(std::string label);
    void check();
    Gtk::Button numbers[10];
};

class WayfireLockerPinPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerPinPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;
    bool disabled = false;

    void update_labels();
    void submit_pin();
    void reset_pin();
    void add_digit(std::string digit);
    std::string sha512(const std::string input);

    std::unordered_map<int, Glib::RefPtr<WayfireLockerPinPluginWidget>> pinpads;

    std::string pin     = "";
    std::string pinhash = "nope";
};
