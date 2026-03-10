#pragma once
#include "gtkmm.h"

enum Stage
{
    PREVIOUS,
    FIRST,
    CONFIRM,
};

class WayfirePinChangeApp : public Gtk::Application
{
    std::string hash_file;
    Gtk::Window window;
    Gtk::Grid grid;
    Gtk::Button numbers[10], cancel, submit;
    Gtk::Label label;

    std::string hash = "", pin = "", confirm = "";
    Stage stage;

  public:
    WayfirePinChangeApp();
    void activate();
    void set_label();
    void pin_key(std::string);
    void pin_cancel();
    void pin_submit();

    std::string sha512(const std::string input);
};
