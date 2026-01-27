#pragma once
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <unordered_map>

#include "plugin.hpp"
#include "lockergrid.hpp"
#include "timedrevealer.hpp"

int pam_conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp,
    void *appdata_ptr);

class WayfireLockerPasswordPluginWidget : public WayfireLockerTimedRevealer
{
  public:
    Gtk::Box box;
    Gtk::Entry entry;
    Gtk::Label label;
    WayfireLockerPasswordPluginWidget(std::string label_contents);
};

class WayfireLockerPasswordPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerPasswordPlugin();
    void add_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void remove_output(int id, std::shared_ptr<WayfireLockerGrid> grid) override;
    void init() override;
    void deinit() override;
    void submit_user_password(std::string password);
    void blank_passwords();

    sigc::connection timeout;
    void update_labels(std::string text);

    std::unordered_map<int, std::shared_ptr<WayfireLockerPasswordPluginWidget>> widgets;
    std::string label_contents     = "";
    std::string submitted_password = "";
};
