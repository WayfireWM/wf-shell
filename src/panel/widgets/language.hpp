#ifndef WIDGETS_LANGUAGE_HPP
#define WIDGETS_LANGUAGE_HPP

#include "../widget.hpp"
#include "gtkmm/button.h"
#include "sigc++/connection.h"
#include "wf-ipc.hpp"
#include <cstdint>
#include <gtkmm/calendar.h>
#include <gtkmm/label.h>
#include <wayfire/nonstd/json.hpp>
#include <string>
#include <vector>

struct Layout
{
    std::string Name;
    std::string ID;
};

class WayfireLanguage : public WayfireWidget, public IIPCSubscriber
{
    // Gtk::Label label;
    Gtk::Button button;
    sigc::connection btn_sig;

    std::shared_ptr<IPCClient> ipc_client;
    uint32_t current_layout;
    std::vector<Layout> available_layouts;

  public:
    void init(Gtk::Box *container);
    void on_event(wf::json_t data) override;
    bool update_label();
    void set_current(uint32_t index);
    void set_available(wf::json_t layouts);
    void next_layout();
    WayfireLanguage();
    ~WayfireLanguage();
};

#endif /* end of include guard: WIDGETS_LANGUAGE_HPP */
