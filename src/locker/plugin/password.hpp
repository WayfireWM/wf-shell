#pragma once
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <unordered_map>

#include "../plugin.hpp"
#include "../../util/wf-option-wrap.hpp"
#include "lockergrid.hpp"

int pam_conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp,
    void *appdata_ptr);

class WayfireLockerPasswordPlugin : public WayfireLockerPlugin
{
  public:
    WayfireLockerPasswordPlugin();
    void add_output(int id, WayfireLockerGrid *grid) override;
    void remove_output(int id) override;
    bool should_enable() override;
    void init() override;
    void submit_user_password(std::string password);
    void blank_passwords();

    WfOption<bool> enable{"locker/password_enable"};

    sigc::connection timeout;
    void update_labels(std::string text);

    std::unordered_map<int, std::shared_ptr<Gtk::Label>> labels;
    std::unordered_map<int, std::shared_ptr<Gtk::Entry>> entries;
    std::string label_contents     = "";
    std::string submitted_password = "";
};
