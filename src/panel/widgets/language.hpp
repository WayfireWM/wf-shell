#ifndef WIDGETS_LANGUAGE_HPP
#define WIDGETS_LANGUAGE_HPP

#include "../widget.hpp"
#include "gtkmm/button.h"
#include "wf-ipc.hpp"
#include <cstdint>
#include <gtkmm/calendar.h>
#include <gtkmm/label.h>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

struct Layout
{
  std::string Name;
  std::string ID;
};

class WayfireLanguage : public WayfireWidget, public IIPCSubscriber
{
    Gtk::Label label;
    Gtk::Button button;

    WayfireIPC *ipc;
    uint32_t current_layout;
    std::vector<Layout> available_layouts;

  public:
    void init(Gtk::HBox *container) override;
    void on_event(nlohmann::json data) override;
    bool update_label();
    void set_current(uint32_t index);
    void set_available(nlohmann::json layouts);
    void next_layout();
    WayfireLanguage(WayfireIPC *ipc);
    ~WayfireLanguage();
};

#endif /* end of include guard: WIDGETS_LANGUAGE_HPP */
