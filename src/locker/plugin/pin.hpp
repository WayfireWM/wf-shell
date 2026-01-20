#pragma once
#include <unordered_map>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/grid.h>
#include <glibmm/refptr.h>

#include "../plugin.hpp"
#include "lockergrid.hpp"

/* Rather than keep an unordered list for each widget, put them together */
class WayfireLockerPinPlugin;
class PinPad : public Gtk::Grid
{
  public:
    PinPad();
    ~PinPad();
    Gtk::Button bsub, bcan;
    Gtk::Label label;
    void init();
    void check();
    Gtk::Button numbers[10];
};

class WayfireLockerPinPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerPinPlugin();
    void add_output(int id, WayfireLockerGrid *grid) override;
    void remove_output(int id) override;
    bool should_enable() override;
    void init() override;

    void update_labels();
    void submit_pin();
    void reset_pin();
    void add_digit(std::string digit);
    std::string sha512(const std::string input);

    bool enable = false;

    std::unordered_map<int, Glib::RefPtr<PinPad>> pinpads;

    std::string pin     = "";
    std::string pinhash = "nope";
};
